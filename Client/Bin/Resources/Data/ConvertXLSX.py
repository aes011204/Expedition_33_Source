import glob
import json
import os

import pandas as pd


BASE_DIR = os.path.dirname(os.path.abspath(__file__))
XLSX_DIR = os.path.join(BASE_DIR, "xlsx")
JSON_DIR = os.path.join(BASE_DIR, "json")


def get_group_name(filename: str) -> str:
    """Return the json root key used by ResourceLoader for a DT_*.xlsx file."""
    stem = os.path.splitext(filename)[0]
    if stem == "DT_Texture":
        return "Texture"
    if stem == "DT_Shader":
        return "DT_Shader"
    return stem


def normalize_value(value):
    """Convert pandas/numpy scalar values into plain json-friendly Python values."""
    if hasattr(value, "item"):
        value = value.item()
    if isinstance(value, float) and value.is_integer():
        return int(value)
    return value


def convert_xlsx_file(xlsx_file: str) -> None:
    """Convert one workbook into one json file, merging all sheets into one root array."""
    filename = os.path.basename(xlsx_file)
    group_name = get_group_name(filename)
    output_path = os.path.join(JSON_DIR, os.path.splitext(filename)[0] + ".json")

    workbook = pd.read_excel(xlsx_file, sheet_name=None, engine="openpyxl")
    file_data = {group_name: []}

    for _, dataframe in workbook.items():
        if dataframe.empty:
            continue

        dataframe = dataframe.dropna(how="all").fillna("")
        for drop_column in ("Memo", "Comment", "주석"):
            if drop_column in dataframe.columns:
                dataframe = dataframe.drop(columns=[drop_column])

        for row in dataframe.to_dict(orient="records"):
            item = {}
            for column_name, value in row.items():
                if str(column_name).startswith("Unnamed"):
                    continue
                if value == "":
                    continue
                item[column_name] = normalize_value(value)

            if item:
                file_data[group_name].append(item)

    os.makedirs(JSON_DIR, exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as json_file:
        json.dump(file_data, json_file, indent=4, ensure_ascii=False)

    print(f"Converted {filename} -> {os.path.basename(output_path)}")


def convert_all_xlsx_to_json() -> None:
    """Convert every DT_*.xlsx file below Data/xlsx into Data/json."""
    os.makedirs(XLSX_DIR, exist_ok=True)
    os.makedirs(JSON_DIR, exist_ok=True)

    xlsx_files = sorted(glob.glob(os.path.join(XLSX_DIR, "DT_*.xlsx")))
    for xlsx_file in xlsx_files:
        if os.path.basename(xlsx_file).startswith("~$"):
            continue
        convert_xlsx_file(xlsx_file)


if __name__ == "__main__":
    convert_all_xlsx_to_json()
