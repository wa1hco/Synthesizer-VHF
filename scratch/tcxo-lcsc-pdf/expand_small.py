#!/usr/bin/env python3
"""Write the LCSC TCXO sheet for SMD5032 and smaller packages.

Source files are the saved list and other-supplier lookups.
A row stays when LCSC stock or other-supplier stock is above zero.
The only temperature collapse is the two ASTX 20 MHz pairs.
"""

import csv
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, "/home/jeff/Documents/Synthesizer-VHF/scratch/tcxo-lcsc-pdf")
from build_sheet import STAB, NT_1KHZ, assign_pn, pn_row, span

ROOT = Path("/home/jeff/Documents/Synthesizer-VHF/scratch/tcxo-lcsc-pdf")
DST = Path("/home/jeff/Documents/Synthesizer-VHF/hardware/datasheets/TCXO/lcsc-smd5032-4p-tcxo.csv")
ODS = DST.with_suffix(".ods")

# Body is 5.0 x 3.2 mm or smaller. Pin count stays in the name.
PACKAGES = {
    "SMD1612-4P",
    "SMD2016-4P",
    "SMD2520-4P",
    "SMD3225-4P",
    "SMD3225-6P",
    "SMD5032-4P",
    "SMD5032-6P",
    "SMD5032-8P",
    "SMD5032-10P",
}

# Part-specific SJK sheets. The electrical table states this nominal frequency.
# The -130 dBc/Hz value sits in the MAX column.
SJK_AT = {
    "7X20000I33XFA": "20",
    "7W25000I33XFA": "25",
    "8W25000P33XCA": "25",
    "8W40000P33XCA": "40",
}

FIELDS = [
    "lcsc", "mfr", "mpn", "package", "freq_MHz", "supply_V", "stability",
    "stability_ppm", "temp_C", "output", "current", "stock_lcsc", "stock_other",
    "other_supplier", "other_price_usd", "other_min_qty", "other_lead_days",
    "price_usd_1", "price_usd_1k",
    "pn_100Hz_dBc", "pn_1kHz_dBc", "pn_10kHz_dBc", "pn_at_MHz", "pn_is_limit",
    "pn_100Hz_10MHz_dBc", "pn_1kHz_10MHz_dBc", "pn_10kHz_10MHz_dBc", "pn_note",
    "catalog", "url",
]


def param_map(product):
    out = {}
    for item in product.get("paramVOList") or []:
        out[item.get("paramNameEn") or ""] = item.get("paramValueEn") or ""
    return out


def freq_num(text):
    text = (text or "").replace("MHz", "").strip()
    try:
        value = float(text)
    except ValueError:
        return text
    if value == int(value):
        return str(int(value))
    return format(value, "g")


def ladder_price(price_list, qty):
    for item in price_list or []:
        if int(item.get("ladder") or 0) == qty:
            return item.get("usdPrice")
    return ""


def lowest_price(price_list):
    best = None
    best_qty = None
    for item in price_list or []:
        price = item.get("usdPrice")
        if price is None:
            continue
        if best is None or float(price) < best:
            best = float(price)
            best_qty = int(item.get("ladder") or 0)
    return best, best_qty


def lead_text(days):
    if not days:
        return ""
    if len(days) == 1:
        return str(days[0])
    return str(days[0]) + "-" + str(days[-1])


def buy_price(row):
    if int(row["stock_lcsc"] or 0) > 0 and row["price_usd_1"] != "":
        return float(row["price_usd_1"])
    if row["other_price_usd"] != "":
        return float(row["other_price_usd"])
    return 1e12


def astx_token(mpn):
    match = re.fullmatch(r"(ASTX-H1[12]-20\.000MHZ)(?:-I25)?-T", mpn)
    if match:
        return match.group(1)
    return None


def group_key(row):
    token = astx_token(row["mpn"])
    if token is None:
        token = "SOLO|" + row["mpn"]
    return "|".join([
        token,
        "stab=" + row["stability"],
        "out=" + row["output"],
        "cur=" + row["current"],
        "f=" + row["freq_MHz"],
        "pkg=" + row["package"],
    ])


def cmos_output(text):
    upper = (text or "").upper()
    if "PECL" in upper or "LVDS" in upper:
        return False
    return "CMOS" in upper


def assign_all(row):
    mpn = row["mpn"]
    freq = row["freq_MHz"]
    if mpn.startswith("8853S"):
        note = "example at 10 MHz"
        if freq != "10":
            note = "example at 10 MHz, not this frequency"
        return pn_row("-120", "-140", "-148", "10", "0", note)
    if mpn == "8122S-25.000NQ33FTL":
        return pn_row(
            "-112", "-133", "-143", "25", "1",
            "maximum at 25 MHz, at +25 C",
        )
    if mpn.startswith(("8121S", "8122S", "8132S", "T132S4")):
        return pn_row(
            "-115", "-135", "-148", "19.2", "0",
            "example at 19.2 MHz, not this frequency",
        )
    if mpn.startswith(("8122H", "8132H", "TC32H")):
        return pn_row(
            "-110", "-130", "-145", "26", "0",
            "example at 26 MHz, not this frequency",
        )
    if mpn.startswith("8132P"):
        return pn_row("", "", "", "", "", "The sheet gives no phase-noise number.")
    if mpn.startswith(("ECS-TXO-2016", "ECS-TXO-2520")):
        return pn_row(
            "", "", "-145", "", "",
            "at 10 kHz. Carrier is not stated.",
        )
    if mpn.startswith("ECS-TXO-3225"):
        return pn_row("", "-135", "", "", "1", "maximum at 1 kHz. Carrier is not stated.")
    if mpn.startswith("ECS-TXO32"):
        if freq != "10":
            return pn_row("", "", "", "", "", "")
        return pn_row(
            "-120", "-140", "-145", "10", "0",
            "typical at 10.000 MHz. Maximum is -115 / -135 / -140.",
        )
    if mpn.startswith("ASTX-"):
        return pn_row(
            "", "-130", "", "", "0",
            "typical at 1 kHz. The value depends on frequency. Carrier is not stated.",
        )
    if mpn in SJK_AT:
        if freq != SJK_AT[mpn]:
            return pn_row("", "", "", "", "", "")
        return pn_row(
            "", "-130", "", freq, "1",
            "maximum at 1 kHz for this part",
        )
    if mpn == "1XTV10000MDA":
        if freq != "10":
            return pn_row("", "", "", "", "", "")
        return pn_row("", "-135", "", "10", "1", "maximum at 1 kHz for this 10 MHz part")
    if mpn.startswith("NT0503C"):
        match = re.fullmatch(r"NT0503C[A-Z]3[A-Z](\d{3})([A-G])N(\d+)", mpn)
        if not match:
            return pn_row("", "", "", "", "", "")
        letter = match.group(2)
        return pn_row(
            "", NT_1KHZ[letter], "", "", "1",
            "order code sets this 1 kHz grade. Carrier is not stated.",
        )
    if mpn.startswith(("MQF326", "VMQF326")):
        if freq == "25" and cmos_output(row["output"]):
            return pn_row(
                "-109", "-125", "-132", "25", "0",
                "typical at 25 MHz. The table output is CMOS.",
            )
        return pn_row(
            "", "", "", "", "",
            "typical table uses other carriers. This row is not scaled.",
        )
    if mpn.startswith("NT0503") and not mpn.startswith("NT0503E"):
        return pn_row("", "", "", "", "", "")
    return assign_pn(row)


def load_rows():
    products = json.loads((ROOT / "tcxo-small.json").read_text())
    third = json.loads((ROOT / "third-small.json").read_text())
    offers = third["offers"]
    errors = third.get("errors") or {}
    rows = []
    unknown_pkg = set()
    unknown_stab = set()
    for index, product in enumerate(products):
        catalog = product.get("catalogName") or ""
        if "TCXO" not in catalog:
            continue
        package = product.get("encapStandard") or ""
        if package not in PACKAGES:
            unknown_pkg.add(package)
            continue
        params = param_map(product)
        code = product["productCode"]
        third_rows = offers.get(code)
        stock_other = 0
        sources = []
        best_price = None
        best_qty = ""
        best_lead = ""
        if third_rows is None:
            other_supplier = "lookup failed"
        elif code in errors and not third_rows:
            other_supplier = "lookup failed"
        else:
            priced = []
            for offer in third_rows:
                stock_other += int(offer.get("stock") or 0)
                source = offer.get("source") or ""
                if source and source not in sources:
                    sources.append(source)
                price, qty = lowest_price(offer.get("prices"))
                if price is None:
                    continue
                priced.append((price, qty, lead_text(offer.get("lead")), source))
            if priced:
                price, qty, lead, _source = sorted(priced)[0]
                best_price = price
                best_qty = qty
                best_lead = lead
            other_supplier = ";".join(sources)
        stock_lcsc = int(product.get("stockNumber") or 0)
        if stock_lcsc <= 0 and stock_other <= 0 and other_supplier != "lookup failed":
            continue
        stability = params.get("Frequency Stability", "")
        if stability not in STAB:
            unknown_stab.add(stability)
        price1 = ladder_price(product.get("productPriceList"), 1)
        rows.append({
            "lcsc": code,
            "mfr": product.get("brandNameEn") or "",
            "mpn": product.get("productModel") or "",
            "package": package,
            "freq_MHz": freq_num(params.get("Frequency")),
            "supply_V": "3.3",
            "stability": stability,
            "temp_C": params.get("Operating Temperature", ""),
            "output": params.get("Output Type", ""),
            "current": params.get("Current - Supply", ""),
            "stock_lcsc": stock_lcsc,
            "stock_other": stock_other,
            "other_supplier": other_supplier,
            "other_price_usd": "" if best_price is None else best_price,
            "other_min_qty": best_qty,
            "other_lead_days": best_lead,
            "price_usd_1": "" if price1 == "" else price1,
            "price_usd_1k": ladder_price(product.get("productPriceList"), 1000),
            "catalog": catalog,
            "url": product.get("url") or "",
            "_i": index,
        })
    if unknown_stab:
        raise SystemExit("stability " + repr(sorted(unknown_stab)))
    return rows, unknown_pkg


def collapse(rows):
    groups = {}
    order = []
    for row in rows:
        key = group_key(row)
        if key not in groups:
            groups[key] = []
            order.append(key)
        groups[key].append(row)
    kept = []
    removed = []
    for key in order:
        members = groups[key]
        members.sort(key=lambda row: (
            buy_price(row),
            -span(row["temp_C"]),
            -(int(row["stock_lcsc"] or 0) + int(row["stock_other"] or 0)),
            row["_i"],
        ))
        kept.append(members[0])
        for row in members[1:]:
            removed.append(row["mpn"])
    return kept, removed


def money(value):
    if value == "" or value is None:
        return ""
    return format(float(value), "g")


def main():
    ods_before = ODS.stat().st_mtime
    old_mpns = set()
    with DST.open(newline="") as handle:
        for row in csv.DictReader(handle):
            old_mpns.add(row["mpn"])
    rows, unknown_pkg = load_rows()
    print("in stock before temp rule", len(rows), "unknown package", unknown_pkg)
    kept, removed = collapse(rows)
    print("temp-dropped", removed)
    # Both rows are other-supplier stock. The narrower temp code has the lower price.
    allowed_drop = {"ASTX-H11-20.000MHZ-I25-T", "ASTX-H12-20.000MHZ-I25-T"}
    if set(removed) != allowed_drop:
        raise SystemExit("unexpected temp drop " + repr(removed))

    out_rows = []
    for row in kept:
        pn = assign_all(row)
        item = {key: row.get(key, "") for key in FIELDS}
        item["stability_ppm"] = STAB.get(row["stability"], "")
        item.update(pn)
        for key in ("price_usd_1", "price_usd_1k", "other_price_usd"):
            item[key] = money(item[key])
        out_rows.append(item)

    out_rows.sort(key=lambda row: (
        float(row["freq_MHz"]),
        (1, 0.0) if row["stability_ppm"] == "" else (0, float(row["stability_ppm"])),
        row["package"],
        buy_price(row),
        row["mpn"],
    ))

    with DST.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=FIELDS, lineterminator="\n")
        writer.writeheader()
        writer.writerows(out_rows)

    with DST.open(newline="") as handle:
        check = list(csv.DictReader(handle))
    if len(check) != len(out_rows):
        raise SystemExit("round trip length")
    if check[0].keys() != out_rows[0].keys() and list(check[0]) != FIELDS:
        raise SystemExit("columns " + repr(list(check[0])))
    if ODS.stat().st_mtime != ods_before:
        raise SystemExit("ods changed")
    if not set(old_mpns) <= {row["mpn"] for row in check}:
        missing = sorted(old_mpns - {row["mpn"] for row in check})
        raise SystemExit("lost old rows " + repr(missing))
    by_mpn = {row["mpn"]: row for row in check}
    board = by_mpn["OW2EL89CEIUXK7YLC-10M"]
    if board["lcsc"] != "C55266004" or board["package"] != "SMD3225-4P":
        raise SystemExit("board part " + repr(board))
    sine = by_mpn["8122S-25.000NQ33FTL"]
    if sine["pn_1kHz_dBc"] != "-133" or sine["pn_is_limit"] != "1" or sine["pn_at_MHz"] != "25":
        raise SystemExit("8122S " + repr(sine))
    s4 = by_mpn["8853S4-t10.000NV33DTL"]
    if s4["pn_100Hz_dBc"] != "-120" or s4["pn_at_MHz"] != "10":
        raise SystemExit("8853S4 " + repr(s4))
    h4 = by_mpn["8853H4-T10.000NQ33DTL"]
    if h4["pn_100Hz_dBc"] != "-115" or h4["pn_at_MHz"] != "19.2":
        raise SystemExit("8853H4 " + repr(h4))
    for row in check:
        if int(row["stock_lcsc"] or 0) + int(row["stock_other"] or 0) <= 0:
            raise SystemExit("zero stock " + row["mpn"])
        if row["package"] not in PACKAGES:
            raise SystemExit("package " + row["package"])
        if row["pn_at_MHz"] == "" and (
            row["pn_100Hz_10MHz_dBc"] or row["pn_1kHz_10MHz_dBc"] or row["pn_10kHz_10MHz_dBc"]
        ):
            raise SystemExit("scaled without carrier " + row["mpn"])

    print("kept", len(check))
    print("lcsc stock", sum(1 for row in check if int(row["stock_lcsc"]) > 0))
    print("other only", sum(1 for row in check if int(row["stock_lcsc"]) == 0))
    from collections import Counter
    print("package", Counter(row["package"] for row in check))
    print("brand", Counter(row["mfr"] for row in check))
    print("cheap")
    for row in sorted(check, key=buy_price)[:8]:
        print(
            f"  ${buy_price(row):<8} lcsc {row['stock_lcsc']:>5} other {row['stock_other']:>6} "
            f"{row['package']:<12} {row['freq_MHz']:>4} ppm {row['stability_ppm']:<5} {row['mpn']}"
        )


if __name__ == "__main__":
    main()
