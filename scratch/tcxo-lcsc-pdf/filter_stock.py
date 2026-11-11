#!/usr/bin/env python3
"""Keep TCXOs that have LCSC stock or other-supplier stock listed on LCSC."""

import csv
import json
import sys
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed

sys.path.insert(0, "/home/jeff/Documents/Synthesizer-VHF/scratch/tcxo-lcsc-pdf")
from build_sheet import STAB, assign_pn, group_key, span

DST = "/home/jeff/Documents/Synthesizer-VHF/hardware/datasheets/TCXO/lcsc-smd5032-4p-tcxo.csv"
HEAD = {
    "Content-Type": "application/json",
    "Origin": "https://www.lcsc.com",
    "Referer": "https://www.lcsc.com/",
    "User-Agent": "Mozilla/5.0",
    "Cookie": "currencyCode=USD",
}


def post(path, payload):
    data = json.dumps(payload).encode()
    req = urllib.request.Request("https://wmsc.lcsc.com" + path, data=data, headers=HEAD)
    with urllib.request.urlopen(req, timeout=40) as resp:
        return json.loads(resp.read().decode())


def list_tcxo():
    body = {
        "currentPage": 1,
        "pageSize": 100,
        "catalogIdList": [1157],
        "encapValueList": ["SMD5032-4P"],
        "paramNameValueMap": {
            "Voltage - Supply": ["3.3V"],
            "Frequency": ["10MHz", "20MHz", "25MHz", "40MHz"],
        },
    }
    prods = []
    page = 1
    while True:
        body["currentPage"] = page
        result = post("/ftps/wm/product/query/list", body)["result"]
        batch = result.get("dataList") or []
        prods.extend(batch)
        if page >= result.get("totalPage", 1) or not batch:
            break
        page += 1
    return [p for p in prods if "TCXO" in (p.get("catalogName") or "")]


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


def third_offers(code):
    last = None
    for _ in range(2):
        try:
            result = post(
                "/ftps/wm/search/third",
                {"productCode": code, "currentPage": 1, "pageSize": 20},
            )
        except Exception as exc:
            last = str(exc)
            continue
        if result.get("code") != 200:
            last = result.get("msg")
            continue
        return result.get("result", {}).get("productList") or [], ""
    return None, last or "lookup failed"


def param_map(product):
    out = {}
    for item in product.get("paramVOList") or []:
        out[item.get("paramNameEn") or ""] = item.get("paramValueEn") or ""
    return out


def freq_num(text):
    text = (text or "").replace("MHz", "").strip()
    if text.endswith(".0"):
        text = text[:-2]
    return text


def lead_text(days):
    if not days:
        return ""
    if len(days) == 1:
        return str(days[0])
    return str(days[0]) + "-" + str(days[-1])


def main():
    products = list_tcxo()
    print("tcxo", len(products), flush=True)
    offers = {}
    errors = {}
    with ThreadPoolExecutor(max_workers=6) as pool:
        futs = {pool.submit(third_offers, p["productCode"]): p["productCode"] for p in products}
        done = 0
        for fut in as_completed(futs):
            code = futs[fut]
            rows, err = fut.result()
            done += 1
            if rows is None:
                errors[code] = err
            else:
                offers[code] = rows
            if done % 40 == 0:
                print("third", done, flush=True)
    print("offers", sum(1 for v in offers.values() if v), "errors", len(errors), flush=True)

    rows = []
    unknown_stab = set()
    for i, product in enumerate(products):
        params = param_map(product)
        code = product["productCode"]
        third = offers.get(code) or []
        stock_other = 0
        sources = []
        best_price = None
        best_qty = ""
        best_lead = ""
        if code in errors and not third:
            other_supplier = "lookup failed"
        else:
            priced = []
            for offer in third:
                stock_other += int(offer.get("stockNumber") or 0)
                source = offer.get("productSource") or offer.get("vendorCode") or ""
                if source and source not in sources:
                    sources.append(source)
                price, qty = lowest_price(offer.get("productPriceList"))
                if price is None:
                    continue
                priced.append((price, qty, lead_text(offer.get("deliveryTimeWayDays")), source))
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
        row = {
            "lcsc": code,
            "mfr": product.get("brandNameEn") or "",
            "mpn": product.get("productModel") or "",
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
            "catalog": product.get("catalogName") or "",
            "url": product.get("url") or "",
            "_i": i,
        }
        rows.append(row)
    print("with stock or failed lookup", len(rows), "unknown stab", unknown_stab, flush=True)

    groups = {}
    order = []
    for row in rows:
        key = group_key(row)
        if key not in groups:
            groups[key] = []
            order.append(key)
        groups[key].append(row)

    def buy_price(row):
        if int(row["stock_lcsc"] or 0) > 0 and row["price_usd_1"] != "":
            return float(row["price_usd_1"])
        if row["other_price_usd"] != "":
            return float(row["other_price_usd"])
        return 1e12

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

    fields = [
        "lcsc", "mfr", "mpn", "freq_MHz", "supply_V", "stability", "stability_ppm",
        "temp_C", "output", "current", "stock_lcsc", "stock_other", "other_supplier",
        "other_price_usd", "other_min_qty", "other_lead_days",
        "price_usd_1", "price_usd_1k",
        "pn_100Hz_dBc", "pn_1kHz_dBc", "pn_10kHz_dBc", "pn_at_MHz", "pn_is_limit",
        "pn_100Hz_10MHz_dBc", "pn_1kHz_10MHz_dBc", "pn_10kHz_10MHz_dBc", "pn_note",
        "catalog", "url",
    ]
    out_rows = []
    for row in kept:
        pn = assign_pn(row)
        item = {k: row.get(k, "") for k in fields}
        item["stability_ppm"] = STAB.get(row["stability"], "")
        item.update(pn)
        for num in ("price_usd_1", "price_usd_1k", "other_price_usd"):
            if item[num] != "" and item[num] is not None:
                item[num] = format(float(item[num]), "g")
        out_rows.append(item)

    with open(DST, "w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, lineterminator="\n", extrasaction="ignore")
        writer.writeheader()
        writer.writerows(out_rows)

    print("kept", len(out_rows), "temp-dropped", len(removed))
    print("lcsc stock", sum(1 for r in out_rows if int(r["stock_lcsc"] or 0) > 0))
    print("other only", sum(1 for r in out_rows if int(r["stock_lcsc"] or 0) == 0 and int(r["stock_other"] or 0) > 0))
    for row in out_rows:
        print(
            f"{row['stock_lcsc']:>6} other {row['stock_other']:>6} {row['other_supplier'][:18]:18} "
            f"${row['price_usd_1'] or '-':>8} {row['stability_ppm'] or '-':>5} {row['freq_MHz']:>4} "
            f"{row['temp_C']:>16} {row['mpn']}"
        )


if __name__ == "__main__":
    main()
