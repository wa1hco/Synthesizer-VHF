#!/usr/bin/env python3
"""Write the TCXO sheet as an ODS file with a color scale on each numeric column.

Green is the low end of that column. Red is the high end. Yellow is the middle.
Stock columns use the reverse scale, so a high stock is green.
The scale uses only the data cells in that column.
"""

import csv
import shutil
import zipfile
from pathlib import Path
from xml.etree import ElementTree as ET

from odf.opendocument import OpenDocumentSpreadsheet
from odf.style import Style, TableColumnProperties, TextProperties
from odf.table import Table, TableCell, TableColumn, TableRow
from odf.text import P

DIR = Path("/home/jeff/Documents/Synthesizer-VHF/hardware/datasheets/TCXO")
CSV_PATH = DIR / "lcsc-smd5032-4p-tcxo.csv"
ODS_PATH = DIR / "lcsc-smd5032-4p-tcxo.ods"
BACKUP_PATH = DIR / "lcsc-smd5032-4p-tcxo-previous.ods"
SHEET = "lcsc-smd5032-4p-tcxo"

# Low number is the favorable end.
LOW_GOOD = {
    "stability_ppm",
    "other_price_usd",
    "price_usd_1",
    "price_usd_1k",
    "pn_100Hz_10MHz_dBc",
    "pn_1kHz_10MHz_dBc",
    "pn_10kHz_10MHz_dBc",
}
# High number is the favorable end.
HIGH_GOOD = {
    "stock_lcsc",
    "stock_other",
}

WIDTHS = {
    "lcsc": "2.6cm",
    "mfr": "3.2cm",
    "mpn": "6.2cm",
    "package": "3.2cm",
    "temp_C": "3.4cm",
    "output": "3.6cm",
    "pn_note": "9.0cm",
    "catalog": "6.0cm",
    "url": "4.0cm",
}
GREEN = "#00a933"
YELLOW = "#ffff00"
RED = "#ff0000"
CALCEXT = "urn:org:documentfoundation:names:experimental:calc:xmlns:calcext:1.0"


def col_letter(index):
    number = index + 1
    letters = ""
    while number:
        number, rem = divmod(number - 1, 26)
        letters = chr(65 + rem) + letters
    return letters


def number_text(text):
    if text is None or text == "":
        return None
    try:
        float(text)
    except ValueError:
        return None
    return text


def color_scale(column, last_row, low_is_green):
    start = f"'{SHEET}'.{column}2"
    end = f"'{SHEET}'.{column}{last_row}"
    if low_is_green:
        low, high = GREEN, RED
    else:
        low, high = RED, GREEN
    return (
        f'<calcext:conditional-format calcext:target-range-address="{start}:{end}">'
        "<calcext:color-scale>"
        f'<calcext:color-scale-entry calcext:value="0" calcext:type="minimum" calcext:color="{low}"/>'
        f'<calcext:color-scale-entry calcext:value="50" calcext:type="percentile" calcext:color="{YELLOW}"/>'
        f'<calcext:color-scale-entry calcext:value="0" calcext:type="maximum" calcext:color="{high}"/>'
        "</calcext:color-scale>"
        "</calcext:conditional-format>"
    )


def build(rows, header):
    doc = OpenDocumentSpreadsheet()
    header_style = Style(name="header", family="table-cell")
    header_style.addElement(TextProperties(fontweight="bold"))
    doc.automaticstyles.addElement(header_style)
    for name, width in WIDTHS.items():
        column_style = Style(name="col-" + name, family="table-column")
        column_style.addElement(TableColumnProperties(columnwidth=width))
        doc.automaticstyles.addElement(column_style)

    table = Table(name=SHEET)
    for name in header:
        if name in WIDTHS:
            table.addElement(TableColumn(stylename="col-" + name))
        else:
            table.addElement(TableColumn())

    head = TableRow()
    for name in header:
        cell = TableCell(valuetype="string", stylename="header")
        cell.addElement(P(text=name))
        head.addElement(cell)
    table.addElement(head)

    for row in rows:
        out = TableRow()
        for name in header:
            text = row.get(name, "")
            number = number_text(text)
            if number is None:
                cell = TableCell(valuetype="string")
                if text:
                    cell.addElement(P(text=text))
            else:
                cell = TableCell(valuetype="float", value=number)
                cell.addElement(P(text=number))
            out.addElement(cell)
        table.addElement(out)
    doc.spreadsheet.addElement(table)

    last_row = len(rows) + 1
    scales = []
    for index, name in enumerate(header):
        if name in LOW_GOOD:
            scales.append(color_scale(col_letter(index), last_row, True))
        elif name in HIGH_GOOD:
            scales.append(color_scale(col_letter(index), last_row, False))
    block = "<calcext:conditional-formats>" + "".join(scales) + "</calcext:conditional-formats>"
    return doc, block


def inject(path, block):
    with zipfile.ZipFile(path, "r") as source:
        files = {name: source.read(name) for name in source.namelist()}
    xml = files["content.xml"].decode("utf-8")
    if "xmlns:calcext=" not in xml:
        xml = xml.replace(
            "<office:document-content ",
            f'<office:document-content xmlns:calcext="{CALCEXT}" ',
            1,
        )
    marker = "</table:table>"
    if marker not in xml:
        raise SystemExit("no table end")
    xml = xml.replace(marker, block + marker, 1)
    files["content.xml"] = xml.encode("utf-8")
    ordered = ["mimetype"] + [name for name in files if name != "mimetype"]
    with zipfile.ZipFile(path, "w") as target:
        for name in ordered:
            compress = zipfile.ZIP_STORED if name == "mimetype" else zipfile.ZIP_DEFLATED
            target.writestr(name, files[name], compress_type=compress)


def check(path, last_row, header):
    xml = zipfile.ZipFile(path).read("content.xml")
    root = ET.fromstring(xml)
    ns = {"calcext": CALCEXT, "table": "urn:oasis:names:tc:opendocument:xmlns:table:1.0",
          "office": "urn:oasis:names:tc:opendocument:xmlns:office:1.0",
          "text": "urn:oasis:names:tc:opendocument:xmlns:text:1.0"}
    formats = root.findall(".//calcext:conditional-format", ns)
    if len(formats) != len(LOW_GOOD) + len(HIGH_GOOD):
        raise SystemExit("scale count " + str(len(formats)))
    letters = {name: col_letter(index) for index, name in enumerate(header)}
    seen = set()
    for item in formats:
        address = item.get(f"{{{CALCEXT}}}target-range-address")
        if not address or not address.endswith(str(last_row)):
            raise SystemExit("range " + repr(address))
        left = address.split(":")[0]
        cell = left.split(".")[-1]
        if not cell.endswith("2"):
            raise SystemExit("range start " + address)
        column = "".join(char for char in cell if char.isalpha())
        seen.add(column)
        colors = [
            entry.get(f"{{{CALCEXT}}}color")
            for entry in item.findall("calcext:color-scale/calcext:color-scale-entry", ns)
        ]
        name = header[letters_index(column)]
        if name in LOW_GOOD and colors != [GREEN, YELLOW, RED]:
            raise SystemExit("low colors " + name + " " + repr(colors))
        if name in HIGH_GOOD and colors != [RED, YELLOW, GREEN]:
            raise SystemExit("high colors " + name + " " + repr(colors))
    expect = {letters[name] for name in LOW_GOOD | HIGH_GOOD}
    if seen != expect:
        raise SystemExit("columns " + repr(sorted(seen)) + " vs " + repr(sorted(expect)))


def letters_index(letters):
    number = 0
    for char in letters:
        number = number * 26 + (ord(char) - 64)
    return number - 1


def main():
    with CSV_PATH.open(newline="") as handle:
        reader = csv.DictReader(handle)
        header = reader.fieldnames
        rows = list(reader)
    if not header or not rows:
        raise SystemExit("empty csv")
    missing = (LOW_GOOD | HIGH_GOOD) - set(header)
    if missing:
        raise SystemExit("missing columns " + repr(sorted(missing)))
    if ODS_PATH.exists() and not BACKUP_PATH.exists():
        shutil.copy2(ODS_PATH, BACKUP_PATH)
    doc, block = build(rows, header)
    doc.save(str(ODS_PATH))
    inject(ODS_PATH, block)
    check(ODS_PATH, len(rows) + 1, header)
    print("rows", len(rows), "scales", len(LOW_GOOD) + len(HIGH_GOOD), "backup", BACKUP_PATH.exists())


if __name__ == "__main__":
    main()
