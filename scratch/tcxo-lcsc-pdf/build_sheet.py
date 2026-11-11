#!/usr/bin/env python3
"""Rebuild the LCSC 5032 TCXO sheet.

Keep the cheapest row when parts differ only by temperature code.
Write stability as ppm. Add phase-noise columns from the family sheets.
"""

import csv
import math
import re
from decimal import Decimal, ROUND_HALF_UP

SRC = "/home/jeff/Documents/Synthesizer-VHF/hardware/datasheets/TCXO/lcsc-smd5032-4p-tcxo.csv"
DST = SRC

STAB = {
    "±50ppb": "0.05",
    "±100ppb": "0.1",
    "±200ppb": "0.2",
    "±280ppb": "0.28",
    "±500ppb": "0.5",
    "±1ppm": "1",
    "±1.5ppm": "1.5",
    "±2ppm": "2",
    "±2.5ppm": "2.5",
    "±5ppm": "5",
    "±15ppm": "15",
    "±25ppm": "25",
    "-": "",
    "": "",
}

# TG5032xFN family typical, TCXO mode. Parentheses in the sheet are VC-TCXO.
EPSON = {
    "10": {"N": ("-125", "-144", "-152"), "D": ("-125", "-144", "-152")},
    "20": {"N": ("-119", "-140", "-152"), "D": ("-117", "-138", "-152")},
    "25": {"N": ("-118", "-139", "-153"), "D": ("-116", "-137", "-152")},
    "40": {"N": ("-114", "-135", "-150"), "D": ("-110", "-132", "-149")},
}

NT_1KHZ = {
    "A": "-120",
    "B": "-125",
    "C": "-130",
    "D": "-135",
    "E": "-140",
    "G": "-145",
}


def one_dec(value):
    q = Decimal(value).quantize(Decimal("0.1"), rounding=ROUND_HALF_UP)
    return format(q, "f")


def scaled(db, carrier_mhz):
    if db == "" or carrier_mhz == "":
        return ""
    f = float(carrier_mhz)
    dbv = float(db)
    if abs(f - 10.0) < 1e-9:
        return one_dec(dbv) if "." in str(db) else str(int(dbv)) if dbv == int(dbv) else one_dec(dbv)
    out = dbv - 20.0 * math.log10(f / 10.0)
    return one_dec(out)


def pn_row(h100, k1, k10, at, limit, note):
    return {
        "pn_100Hz_dBc": h100,
        "pn_1kHz_dBc": k1,
        "pn_10kHz_dBc": k10,
        "pn_at_MHz": at,
        "pn_is_limit": limit,
        "pn_100Hz_10MHz_dBc": scaled(h100, at),
        "pn_1kHz_10MHz_dBc": scaled(k1, at),
        "pn_10kHz_10MHz_dBc": scaled(k10, at),
        "pn_note": note,
    }


def empty_pn():
    return pn_row("", "", "", "", "", "")


def assign_pn(row):
    mpn = row["mpn"]
    freq = row["freq_MHz"]
    if mpn.startswith("TG5032"):
        m = re.search(r"M-(.{5})", mpn)
        vc = m.group(1)[4]
        h100, k1, k10 = EPSON[freq][vc]
        mode = "TCXO mode" if vc == "N" else "VC-TCXO mode"
        note = f"TG5032 family typical, {mode}"
        if freq == "10" and vc == "D":
            note = "TG5032 family typical. The 10 MHz table has one value. This code has Vc."
        return pn_row(h100, k1, k10, freq, "0", note)
    if mpn.startswith("AST3TQ53"):
        note = "maximum at 10 MHz"
        if freq != "10":
            note = "family maximum at 10 MHz, not this frequency"
        return pn_row("-120", "-140", "-145", "10", "1", note)
    if mpn.startswith("AST3TDA"):
        return pn_row(
            "-120", "-145", "-157", "20", "0",
            "typical at 20 MHz. Maximum is -115 / -140 / -152.",
        )
    if mpn.startswith("580L"):
        if freq == "40":
            return pn_row(
                "-113.0", "-135.5", "-149.7", "40", "0",
                "typical plot at 40 MHz. Caption includes Vc = 1.5 V. Model 581 shows the same plot.",
            )
        return pn_row(
            "-120.7", "-140.7", "-155.7", "24.576", "0",
            "typical plot at 24.576 MHz, not this frequency. Model 581 shows the same plot.",
        )
    if mpn.startswith("581L"):
        if freq == "40":
            return pn_row(
                "-113.0", "-135.5", "-149.7", "40", "0",
                "typical plot at 40 MHz. Model 580 uses this plot and states Vc = 1.5 V.",
            )
        return pn_row(
            "-120.7", "-140.7", "-155.7", "24.576", "0",
            "typical plot at 24.576 MHz, not this frequency. Same plot as model 580.",
        )
    if mpn.startswith(("8153H", "8153S", "8853", "9153")):
        return pn_row(
            "-115", "-135", "-148", "19.2", "0",
            "example at 19.2 MHz, not this frequency",
        )
    if mpn.startswith("TH53"):
        return pn_row(
            "-115", "-135", "-148", "40", "0",
            "typical for this 40 MHz part. Maximum is -110 / -130 / -145.",
        )
    if mpn.startswith("STC53K"):
        return pn_row(
            "-115", "-135", "-145", "", "0",
            "STC53K typical. The table does not state the carrier. Plots are at 19.44 MHz and 40 MHz.",
        )
    if mpn.startswith("STC53C"):
        return pn_row(
            "-115", "-135", "-145", "", "0",
            "STC53C typical. The table does not state the carrier. Plots are at 19.44 MHz and 40 MHz.",
        )
    if mpn.startswith("FT5H"):
        return pn_row("", "-130", "", "", "0", "typical at 1 kHz. Carrier is not stated.")
    if mpn.startswith("ECS-TXO-5032"):
        return pn_row("", "-135", "", "", "1", "maximum at 1 kHz. Carrier is not stated.")
    if mpn.startswith(("TX-T", "VTX-")):
        return pn_row("", "-130", "", "13", "1", "maximum at 13 MHz, not this frequency")
    if mpn.startswith("NT0503"):
        m = re.fullmatch(r"NT0503EH3I(\d{3})([A-G])N(\d+)", mpn)
        if not m:
            raise SystemExit("NT0503 code " + mpn)
        letter = m.group(2)
        if freq == "10" and letter == "E":
            return pn_row("-120", "-140", "-145", "10", "1", "maximum at 10 MHz")
        note = "order code sets this 1 kHz grade. 100 Hz and 10 kHz are not stated."
        if freq == "40" and letter == "B":
            note = "order code sets -125 dBc/Hz at 1 kHz. The chart marks this grade as not offered at 40 MHz and above."
        return pn_row("", NT_1KHZ[letter], "", freq, "1", note)
    if mpn.startswith("TWE"):
        note = "TW type limit at 10 MHz"
        if freq != "10":
            note = "TW type limit at 10 MHz, not this frequency"
        return pn_row("-125", "-145", "-150", "10", "1", note)
    return empty_pn()


def cts_key(mpn):
    m = re.fullmatch(r"(58[01]L)(\d+)(02|X2|05)([A-Z])(TT|AT)", mpn)
    if not m:
        return None
    series, freqcode, stab, _temp, pkg = m.groups()
    return f"CTS|{series}|{freqcode}|{stab}|{pkg}"


def sun_key(mpn):
    m = re.fullmatch(r"STC53([CK])33([A-Z])(\d{2})-20\.000M", mpn)
    if not m:
        return None
    out, stab, _temp = m.groups()
    return f"SUN|STC53{out}33{stab}"


def group_key(row):
    mpn = row["mpn"]
    key = None
    if row["mfr"].startswith("CTS"):
        key = cts_key(mpn)
    elif row["mfr"].startswith("Suntsu"):
        key = sun_key(mpn)
    if key is None:
        key = "SOLO|" + mpn
    return "|".join([
        key,
        "stab=" + row["stability"],
        "out=" + row["output"],
        "cur=" + row["current"],
        "f=" + row["freq_MHz"],
    ])


def span(temp):
    m = re.search(r"([+-]?\d+)℃~([+-]?\d+)℃", temp)
    if not m:
        return 0
    return int(m.group(2)) - int(m.group(1))


def main():
    with open(SRC, newline="") as f:
        rows = list(csv.DictReader(f))
    groups = {}
    order = []
    for i, row in enumerate(rows):
        if row["stability"] not in STAB:
            raise SystemExit("stability " + repr(row["stability"]))
        key = group_key(row)
        if key not in groups:
            groups[key] = []
            order.append(key)
        groups[key].append((i, row))

    kept = []
    removed = []
    for key in order:
        members = groups[key]
        members_sorted = sorted(
            members,
            key=lambda item: (
                float(item[1]["price_usd_1"]),
                -span(item[1]["temp_C"]),
                -int(item[1]["stock_lcsc"] or 0),
                item[0],
            ),
        )
        _i, win = members_sorted[0]
        kept.append(win)
        for item in members_sorted[1:]:
            removed.append(item[1]["mpn"])

    fields = [
        "lcsc", "mfr", "mpn", "freq_MHz", "supply_V", "stability", "stability_ppm",
        "temp_C", "output", "current", "stock_lcsc", "price_usd_1", "price_usd_1k",
        "pn_100Hz_dBc", "pn_1kHz_dBc", "pn_10kHz_dBc", "pn_at_MHz", "pn_is_limit",
        "pn_100Hz_10MHz_dBc", "pn_1kHz_10MHz_dBc", "pn_10kHz_10MHz_dBc", "pn_note",
        "catalog", "url",
    ]
    out_rows = []
    for row in kept:
        pn = assign_pn(row)
        item = {
            "lcsc": row["lcsc"],
            "mfr": row["mfr"],
            "mpn": row["mpn"],
            "freq_MHz": row["freq_MHz"],
            "supply_V": row["supply_V"],
            "stability": row["stability"],
            "stability_ppm": STAB[row["stability"]],
            "temp_C": row["temp_C"],
            "output": row["output"],
            "current": row["current"],
            "stock_lcsc": row["stock_lcsc"],
            "price_usd_1": row["price_usd_1"],
            "price_usd_1k": row["price_usd_1k"],
            "catalog": row["catalog"],
            "url": row["url"],
        }
        item.update(pn)
        out_rows.append(item)

    with open(DST, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields, lineterminator="\n")
        w.writeheader()
        w.writerows(out_rows)

    print("in", len(rows), "kept", len(out_rows), "removed", len(removed))
    print("groups", len(groups), "multi", sum(1 for v in groups.values() if len(v) > 1))
    with_1k = sum(1 for r in out_rows if r["pn_1kHz_dBc"])
    with_scale = sum(1 for r in out_rows if r["pn_1kHz_10MHz_dBc"])
    print("pn_1kHz", with_1k, "scaled_1kHz", with_scale, "no_pn", len(out_rows) - with_1k)
    # spot checks
    by_mpn = {r["mpn"]: r for r in out_rows}
    checks = {
        "TG5032CFN 40.0000M-CBGNNAB": ("-126.0", "-147.0", "-162.0"),
        "TG5032CFN 10.0000M-CAGNNAB": ("-125", "-144", "-152"),
        "TG5032SFN 20.0000M-CAGNDAB": ("-123.0", "-144.0", "-158.0"),
        "TG5032CFN 25.0000M-CAGNNAB": ("-126.0", "-147.0", "-161.0"),
        "8153H-10.000MK33DTL": ("-120.7", "-140.7", "-153.7"),
        "AST3TQ53-T-40.000MHZ-5-SW-T2": ("-120", "-140", "-145"),
        "AST3TDA53TACJ2-20.0000MHZ": ("-126.0", "-151.0", "-163.0"),
        "TH53H4-40.000NQ33DTL": ("-127.0", "-147.0", "-160.0"),
        "STC53C33R07-20.000M": ("", "", ""),
        "NT0503EH3I507EN10": ("-120", "-140", "-145"),
    }
    for mpn, expect in checks.items():
        r = by_mpn[mpn]
        got = (r["pn_100Hz_10MHz_dBc"], r["pn_1kHz_10MHz_dBc"], r["pn_10kHz_10MHz_dBc"])
        print(mpn, got, "OK" if got == expect else "DIFF want " + str(expect), "ppm", r["stability_ppm"], r["temp_C"], r["price_usd_1"])
    # cheapest suntsu and cts tie
    print("sun R", by_mpn["STC53C33R07-20.000M"]["temp_C"], by_mpn["STC53C33R07-20.000M"]["price_usd_1"])
    print("cts", by_mpn["580L10002ITT"]["temp_C"], by_mpn["580L10002ITT"]["price_usd_1"])
    assert "580L10002CTT" not in by_mpn
    assert "STC53C33R48-20.000M" not in by_mpn
    assert "AST3TQ53-V-10.000MHZ-5-C-T5" in by_mpn
    assert "AST3TQ53-V-10.000MHZ-5-C-T2" in by_mpn
    assert len(out_rows) == 164


if __name__ == "__main__":
    main()
