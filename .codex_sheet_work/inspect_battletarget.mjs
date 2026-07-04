import fs from "node:fs/promises";
import path from "node:path";
import { FileBlob, SpreadsheetFile } from "@oai/artifact-tool";

const workbookPath = "C:/Users/a9018/Downloads/World.xlsx";
const outputDir = "C:/Users/a9018/Desktop/Expedition_158/.codex_sheet_work";

const input = await FileBlob.load(workbookPath);
const workbook = await SpreadsheetFile.importXlsx(input);

const sheetSummary = await workbook.inspect({
  kind: "sheet",
  include: "id,name",
  maxChars: 6000,
});
console.log("SHEETS");
console.log(sheetSummary.ndjson);

const overview = await workbook.inspect({
  kind: "workbook,sheet,table",
  tableMaxRows: 4,
  tableMaxCols: 8,
  tableMaxCellChars: 80,
  maxChars: 12000,
});
console.log("OVERVIEW");
console.log(overview.ndjson);

const region = await workbook.inspect({
  kind: "region",
  sheetId: "BattleTarget",
  range: "A1:Z80",
  maxChars: 20000,
});
console.log("BATTLETARGET_REGION");
console.log(region.ndjson);

const style = await workbook.inspect({
  kind: "computedStyle",
  sheetId: "BattleTarget",
  range: "A1:Z8",
  maxChars: 8000,
});
console.log("BATTLETARGET_STYLE");
console.log(style.ndjson);

const preview = await workbook.render({
  sheetName: "BattleTarget",
  autoCrop: "all",
  scale: 1,
  format: "png",
});
await fs.writeFile(path.join(outputDir, "BattleTarget_preview.png"), new Uint8Array(await preview.arrayBuffer()));
