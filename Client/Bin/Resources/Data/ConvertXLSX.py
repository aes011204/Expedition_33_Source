import argparse
import glob
import json
import os
import shutil
import tempfile
import urllib.error
import urllib.request

import pandas as pd


BASE_DIR = os.path.dirname(os.path.abspath(__file__))
XLSX_DIR = os.path.join(BASE_DIR, "xlsx")
JSON_DIR = os.path.join(BASE_DIR, "json")
GOOGLE_SHEET_EXPORTS = {
    "DT_Component.xlsx": "https://docs.google.com/spreadsheets/d/1RjljR3NNALOuZXTwCUI-9B4L5PvhrKE96d--o6eSeYc/export?format=xlsx",
    "DT_GameObject.xlsx": "https://docs.google.com/spreadsheets/d/16kVJyaLRmilyXw1EvpNuFG16Pn6i-j4SUFi9nLPthX4/export?format=xlsx",
    "DT_Model.xlsx": "https://docs.google.com/spreadsheets/d/13uyF_Cv9TqTg2PwH_t5MY1QYE48AxiQV39lwzPZObAw/export?format=xlsx",
    "DT_Shader.xlsx": "https://docs.google.com/spreadsheets/d/1qSDR_FEbkFcCl2SNlVH_d54kjiuoj1_OQiRRk7YzQ9I/export?format=xlsx",
    "DT_StaticLevel.xlsx": "https://docs.google.com/spreadsheets/d/13y0vauJio_y14v3CFpEh4FjHzYivTFbl11AEIsBZrV8/export?format=xlsx",
    "DT_Texture.xlsx": "https://docs.google.com/spreadsheets/d/1UuiYVCXSMiU_Rv3d7OpNFRE2Gp2Lo8gfV4_ZCIy5xms/export?format=xlsx",
    "DT_ExpLevel.xlsx": "https://docs.google.com/spreadsheets/d/1tGnts2HdLZXOccMK21symSV6jAdooHCckLeEtwOz97Y/export?format=xlsx",
    "DT_Weapon.xlsx": "https://docs.google.com/spreadsheets/d/1209yLBKX6EXPQ4Zsgy1kd6-O6ilofhKCuhA279U9Res/export?format=xlsx",
    "DT_Item.xlsx": "https://docs.google.com/spreadsheets/d/17iQZ30XSCVYyXoZLXvf5bGsVv8H4JPs_Lw3Si5odjG4/export?format=xlsx",
    "DT_SkillTree.xlsx": "https://docs.google.com/spreadsheets/d/19--V-7P5UoZd7SRJ3vdMl235gXcka5eVsyjFxOHVHoY/export?format=xlsx",
    "DT_SkillTreeRequirement.xlsx": "https://docs.google.com/spreadsheets/d/1Xxqg_rbp6sAq_PWSUxP6iRek7Itfas9zqrgQE-WQVtk/export?format=xlsx",
    "DT_Skill.xlsx": "https://docs.google.com/spreadsheets/d/1ZYbFBUcuUSYvSQR8D_o_Z95pZRv2HLopIpwxryuCo4Q/export?format=xlsx",
    "DT_Skill_Info.xlsx": "https://docs.google.com/spreadsheets/d/17SLPwvqcSqObjfzEFZymtw2qsnNHvRnS6U1WDk0JNdY/export?format=xlsx",
    "DT_SkillAction.xlsx": "https://docs.google.com/spreadsheets/d/1Zgifm1ACD1RdCZdtqT5FhCy02dZJK663VcyNEWxtCts/export?format=xlsx",
    "DT_SoundCue.xlsx": "https://docs.google.com/spreadsheets/d/1oG7Imr8t_kLeE8lyCwggafz1gIinoa69icHQF5SwFww/export?format=xlsx",
}


def get_google_sheet_edit_url(export_url: str) -> str:
    """Convert a workbook export URL into a browser-edit URL."""
    return export_url.replace("/export?format=xlsx", "/edit?gid=0#gid=0")


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


def sync_google_sheet_xlsx(filename: str, export_url: str) -> str:
    """Download the latest workbook export before conversion."""
    os.makedirs(XLSX_DIR, exist_ok=True)

    target_path = os.path.join(XLSX_DIR, filename)
    fd, temp_path = tempfile.mkstemp(suffix=".xlsx", dir=XLSX_DIR)
    os.close(fd)

    try:
        with urllib.request.urlopen(export_url) as response, open(temp_path, "wb") as temp_file:
            shutil.copyfileobj(response, temp_file)

        if os.path.exists(target_path):
            with open(temp_path, "rb") as new_file, open(target_path, "rb") as current_file:
                if new_file.read() == current_file.read():
                    os.remove(temp_path)
                    print(f"Synced {filename} (unchanged)")
                    return target_path

        os.replace(temp_path, target_path)
        print(f"Synced {filename}")
        return target_path
    except Exception:
        if os.path.exists(temp_path):
            os.remove(temp_path)
        raise


def sync_google_sheet_shortcuts() -> None:
    """Refresh local .url shortcuts for every synced Google Sheet."""
    os.makedirs(XLSX_DIR, exist_ok=True)

    for filename, export_url in GOOGLE_SHEET_EXPORTS.items():
        shortcut_name = os.path.splitext(filename)[0] + "_GoogleSheet.url"
        shortcut_path = os.path.join(XLSX_DIR, shortcut_name)
        shortcut_contents = (
            "[InternetShortcut]\n"
            f"URL={get_google_sheet_edit_url(export_url)}\n"
        )

        with open(shortcut_path, "w", encoding="ascii", newline="\n") as shortcut_file:
            shortcut_file.write(shortcut_contents)

        print(f"Synced {shortcut_name}")


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


def list_local_xlsx_files() -> list[str]:
    """Return local DT_*.xlsx files in a stable order, excluding temp lock files."""
    os.makedirs(XLSX_DIR, exist_ok=True)

    xlsx_files = sorted(glob.glob(os.path.join(XLSX_DIR, "DT_*.xlsx")))
    return [
        xlsx_file
        for xlsx_file in xlsx_files
        if not os.path.basename(xlsx_file).startswith("~$")
    ]


def convert_all_xlsx_to_json(selected_sheet: str | None = None) -> None:
    """Convert every DT_*.xlsx file, or one selected workbook, into Data/json."""
    os.makedirs(JSON_DIR, exist_ok=True)

    xlsx_files = list_local_xlsx_files()
    if selected_sheet:
        selected_path = os.path.join(XLSX_DIR, selected_sheet)
        if selected_path not in xlsx_files:
            raise FileNotFoundError(f"Selected sheet not found: {selected_sheet}")
        xlsx_files = [selected_path]

    for xlsx_file in xlsx_files:
        convert_xlsx_file(xlsx_file)


def sync_google_sheets(selected_sheet: str | None = None) -> None:
    """Refresh local xlsx snapshots from Google Sheets."""
    sync_google_sheet_shortcuts()

    sync_targets = GOOGLE_SHEET_EXPORTS.items()
    if selected_sheet:
        export_url = GOOGLE_SHEET_EXPORTS.get(selected_sheet)
        if export_url is None:
            print(f"Skipped {selected_sheet} sync (Google Sheet URL not registered)")
            return
        sync_targets = [(selected_sheet, export_url)]

    for filename, export_url in sync_targets:
        try:
            sync_google_sheet_xlsx(filename, export_url)
        except urllib.error.HTTPError as error:
            print(f"Skipped {filename} sync (HTTP {error.code}): {get_google_sheet_edit_url(export_url)}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert local DT_*.xlsx files into json resources."
    )
    parser.add_argument(
        "--sync-google",
        action="store_true",
        help="Download the latest Google Sheet exports into Data/xlsx before conversion.",
    )
    parser.add_argument(
        "--sheet",
        help="Convert only one DT_*.xlsx filename from Data/xlsx.",
    )
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()
    if args.sync_google:
        sync_google_sheets(args.sheet)
    convert_all_xlsx_to_json(args.sheet)
