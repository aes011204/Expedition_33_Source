import fs from "node:fs/promises";
import path from "node:path";
import { FileBlob, SpreadsheetFile } from "@oai/artifact-tool";

const workbookPath = "C:/Users/a9018/Downloads/World.xlsx";
const soundRoot = "C:/Users/a9018/Desktop/Expedition_158/Client/Bin/Resources/Sounds";
const battleTargetRoot = path.join(soundRoot, "Battle", "Character", "BattleTargeting");
const outputDir = "C:/Users/a9018/Desktop/Expedition_158/outputs/battletarget";
const outputPath = path.join(outputDir, "World_BattleTarget.xlsx");

const heroes = ["Gustave", "Lune", "Maelle"];

async function listFiles(dir) {
  const entries = await fs.readdir(dir, { withFileTypes: true });
  const files = [];
  for (const entry of entries) {
    const fullPath = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      files.push(...await listFiles(fullPath));
    } else if (entry.isFile() && entry.name.toLowerCase().endsWith(".wav")) {
      files.push(fullPath);
    }
  }
  return files;
}

function toSoundPath(fullPath) {
  return path.relative(soundRoot, fullPath).replace(/\\/g, "/");
}

function soundList(files) {
  return files.map(toSoundPath).sort((a, b) => a.localeCompare(b, undefined, { numeric: true })).join("|");
}

function findFiles(allFiles, speaker, pattern) {
  const speakerSegment = `${path.sep}${speaker}${path.sep}`;
  return allFiles.filter((filePath) => filePath.includes(speakerSegment) && pattern.test(path.basename(filePath)));
}

function makeRow(cueId, sounds, memo) {
  return [
    cueId,
    "Player",
    true,
    3,
    35,
    1,
    1,
    1,
    1,
    sounds,
    memo,
  ];
}

const allFiles = await listFiles(battleTargetRoot);

const specific = (speaker, target) =>
  soundList(findFiles(allFiles, speaker, new RegExp(`^VO_BL_Battle_EnemyTarget_${target}_\\d+_${speaker} \\[1\\]\\.wav$`)));

const self = (hero) =>
  soundList(findFiles(allFiles, hero, new RegExp(`^VO_BL_Battle_EnemyTargetSelf_\\d+_${hero} \\[1\\]\\.wav$`)));

const allWarning = (target) =>
  soundList(heroes
    .filter((speaker) => speaker !== target)
    .flatMap((speaker) => findFiles(
      allFiles,
      speaker,
      new RegExp(`^VO_BL_Battle_EnemyTargetOtherChar_\\d+_${speaker} \\[1\\]\\.wav$`),
    )));

const rows = [
  makeRow("Battle_MonsterTarget_Warning_Lune_To_Gustave", specific("Lune", "Gustave"), "몬스터가 Gustave를 타겟팅할 때 Lune 경고 VO"),
  makeRow("Battle_MonsterTarget_Warning_Maelle_To_Gustave", specific("Maelle", "Gustave"), "몬스터가 Gustave를 타겟팅할 때 Maelle 경고 VO"),
  makeRow("Battle_MonsterTarget_Warning_All_To_Gustave", allWarning("Gustave"), "몬스터가 Gustave를 타겟팅할 때 전체 Warning VO"),

  makeRow("Battle_MonsterTarget_Warning_Gustave_To_Lune", specific("Gustave", "Lune"), "몬스터가 Lune을 타겟팅할 때 Gustave 경고 VO"),
  makeRow("Battle_MonsterTarget_Warning_Maelle_To_Lune", specific("Maelle", "Lune"), "몬스터가 Lune을 타겟팅할 때 Maelle 경고 VO"),
  makeRow("Battle_MonsterTarget_Warning_All_To_Lune", allWarning("Lune"), "몬스터가 Lune을 타겟팅할 때 전체 Warning VO"),
  makeRow("Battle_MonsterTarget_SelfTargeting_Lune", self("Lune"), "몬스터가 Lune을 타겟팅할 때 Lune SelfTargeting VO"),

  makeRow("Battle_MonsterTarget_Warning_Gustave_To_Maelle", specific("Gustave", "Maelle"), "몬스터가 Maelle을 타겟팅할 때 Gustave 경고 VO"),
  makeRow("Battle_MonsterTarget_Warning_Lune_To_Maelle", specific("Lune", "Maelle"), "몬스터가 Maelle을 타겟팅할 때 Lune 경고 VO"),
  makeRow("Battle_MonsterTarget_Warning_All_To_Maelle", allWarning("Maelle"), "몬스터가 Maelle을 타겟팅할 때 전체 Warning VO"),
  makeRow("Battle_MonsterTarget_SelfTargeting_Maelle", self("Maelle"), "몬스터가 Maelle을 타겟팅할 때 Maelle SelfTargeting VO"),
];

const missing = rows.filter((row) => !row[9]);
if (missing.length > 0) {
  throw new Error(`Missing sounds for cue(s): ${missing.map((row) => row[0]).join(", ")}`);
}

const input = await FileBlob.load(workbookPath);
const workbook = await SpreadsheetFile.importXlsx(input);
const sheet = workbook.worksheets.getItem("BattleTarget");

for (const table of sheet.tables.items ?? []) {
  table.delete();
}

const headers = [["cueId", "channel", "spatial", "minDistance", "maxDistance", "volumeMin", "volumeMax", "pitchMin", "pitchMax", "sounds", "Memo"]];
const endRow = rows.length + 1;
sheet.getRange(`A1:K${Math.max(endRow, 200)}`).clear({ applyTo: "all" });
sheet.getRange(`A1:K${endRow}`).values = [...headers, ...rows];

sheet.getRange("A1:K1").format = {
  fill: "#1F4E78",
  font: { bold: true, color: "#FFFFFF" },
  horizontalAlignment: "center",
  borders: { preset: "all", style: "thin", color: "#95B3D7" },
};
sheet.getRange(`A2:K${endRow}`).format = {
  font: { fontSize: 10 },
  verticalAlignment: "center",
  borders: { preset: "all", style: "thin", color: "#D9E2F3" },
};
sheet.getRange(`A2:B${endRow}`).format.horizontalAlignment = "left";
sheet.getRange(`C2:I${endRow}`).format.horizontalAlignment = "center";
sheet.getRange(`J2:K${endRow}`).format.horizontalAlignment = "left";
sheet.getRange(`A1:A${endRow}`).format.columnWidth = 48;
sheet.getRange(`B1:B${endRow}`).format.columnWidth = 12;
sheet.getRange(`C1:I${endRow}`).format.columnWidth = 12;
sheet.getRange(`J1:J${endRow}`).format.columnWidth = 120;
sheet.getRange(`K1:K${endRow}`).format.columnWidth = 42;
sheet.getRange(`A2:K${endRow}`).format.rowHeight = 26;
sheet.freezePanes.freezeRows(1);
sheet.showGridLines = false;

const table = sheet.tables.add(`A1:K${endRow}`, true, "BattleTargetTable");
table.style = "TableStyleMedium2";
table.showFilterButton = true;

const check = await workbook.inspect({
  kind: "table",
  sheetId: "BattleTarget",
  range: `A1:K${endRow}`,
  include: "values",
  tableMaxRows: 14,
  tableMaxCols: 11,
  tableMaxCellChars: 80,
  maxChars: 12000,
});
console.log(check.ndjson);

const errors = await workbook.inspect({
  kind: "match",
  searchTerm: "#REF!|#DIV/0!|#VALUE!|#NAME\\?|#N/A",
  options: { useRegex: true, maxResults: 100 },
  summary: "formula error scan",
});
console.log(errors.ndjson);

await fs.mkdir(outputDir, { recursive: true });
const preview = await workbook.render({
  sheetName: "BattleTarget",
  autoCrop: "all",
  scale: 1,
  format: "png",
});
await fs.writeFile(path.join(outputDir, "BattleTarget_preview.png"), new Uint8Array(await preview.arrayBuffer()));

const output = await SpreadsheetFile.exportXlsx(workbook);
await output.save(outputPath);

console.log(`Saved ${outputPath}`);
