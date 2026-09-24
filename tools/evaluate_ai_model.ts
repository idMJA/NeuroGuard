import * as fs from "fs";
import * as path from "path";

// Implementasi logika MLP 11-dimensi persis seperti di stroke_ml_model.dart
const featureMeans = [1.8, 0.35, 0.25, 76.0, 97.5, 0.0, 65.0, 0.4, 0.3, 0.2, 0.15];
const featureStd   = [1.2, 0.2,  0.15, 16.0, 3.0,  0.3, 12.0, 0.49,0.46,0.40,0.36];

const w1 = [
  [ 0.85,  0.72,  0.45,  0.60,  0.15,  0.50,  0.78,  0.65],
  [ 0.70,  0.55,  0.30,  0.40,  0.10,  0.35,  0.60,  0.50],
  [ 0.35,  0.25,  0.15,  0.20,  0.05,  0.15,  0.30,  0.25],
  [ 0.20,  0.45,  0.65,  0.10,  0.70,  0.15,  0.25,  0.30],
  [-0.60, -0.75, -0.40, -0.50, -0.80, -0.30, -0.55, -0.65],
  [ 1.40,  1.20,  1.10,  1.50,  0.90,  1.30,  1.25,  1.35],
  [ 0.15,  0.20,  0.18,  0.12,  0.22,  0.14,  0.16,  0.19],
  [ 0.30,  0.28,  0.35,  0.25,  0.40,  0.22,  0.32,  0.36],
  [ 0.25,  0.22,  0.28,  0.20,  0.30,  0.18,  0.24,  0.27],
  [ 0.40,  0.38,  0.50,  0.35,  0.55,  0.30,  0.42,  0.48],
  [ 0.50,  0.45,  0.40,  0.38,  0.42,  0.35,  0.48,  0.44],
];
const b1 = [-0.2, -0.15, -0.1, -0.25, -0.1, -0.2, -0.18, -0.15];
const w2 = [0.42, 0.38, 0.35, 0.30, 0.45, 0.28, 0.40, 0.36];
const b2 = -1.65;

function sigmoid(z: number) { return 1.0 / (1.0 + Math.exp(-z)); }
function relu(x: number) { return x > 0 ? x : 0.0; }

function predictMl(
  dar: number, 
  sq: number, 
  hasStrokeHistory: boolean, 
  hasHypertension: boolean = true, 
  patientAge: number = 70
) {
  const cleanDar = (sq <= 50 && dar > 0) ? dar : 1.0;
  const deltaRatio = (cleanDar > 2.5) ? 0.65 : 0.25;
  const thetaRatio = 0.20;
  const hr = 75.0;
  const spo2 = 98.0;
  const fall = 0.0;

  const rawFeatures = [
    cleanDar,
    deltaRatio,
    thetaRatio,
    hr,
    spo2,
    fall,
    patientAge,
    hasHypertension ? 1.0 : 0.0,
    0.0, // Diabetes baseline
    0.0, // AFib
    hasStrokeHistory ? 1.0 : 0.0
  ];

  // Z-Score Standardize
  const norm = rawFeatures.map((f, i) => (f - featureMeans[i]) / featureStd[i]);

  // Forward Pass Layer 1
  const hidden: number[] = [];
  for (let j = 0; j < 8; j++) {
    let sum = b1[j];
    for (let i = 0; i < 11; i++) {
      sum += norm[i] * w1[i][j];
    }
    hidden.push(relu(sum));
  }

  // Forward Pass Layer 2
  let logit = b2;
  for (let j = 0; j < 8; j++) {
    logit += hidden[j] * w2[j];
  }

  const prob = sigmoid(logit);
  const score = Math.round(prob * 100);
  let riskClass = "LOW";
  if (score >= 65) riskClass = "CRITICAL";
  else if (score >= 35) riskClass = "ELEVATED";

  return { prob, score, riskClass };
}

// Baca dataset clean yang baru diekspor
const datasetPath = path.join(process.cwd(), "data", "dataset_pslugs_20260923", "dataset_pslugs_clean_eeg.csv");
const lines = fs.readFileSync(datasetPath, "utf-8").trim().split("\n");
const header = lines[0].split(",");

const subjects: Record<string, { notes: string; scores: number[]; classes: Record<string, number> }> = {};

for (let i = 1; i < lines.length; i++) {
  const cols = lines[i].split(",");
  const id = cols[0].replace(/"/g, "");
  const notes = cols[2].replace(/"/g, "");
  const sq = parseInt(cols[18]);
  const dar = parseFloat(cols[19]);
  const hasHistory = notes.includes("Riwayat Stroke");

  const res = predictMl(dar, sq, hasHistory);

  if (!subjects[id]) {
    subjects[id] = { notes, scores: [], classes: { LOW: 0, ELEVATED: 0, CRITICAL: 0 } };
  }
  subjects[id].scores.push(res.score);
  subjects[id].classes[res.riskClass]++;
}

console.log("================================================================================");
console.log("           EVALUASI MODEL AI NEURAL NETWORK (Flutter stroke_ml_model)            ");
console.log("                    PADA DATASET LANSIA UPTD PSLUGS                             ");
console.log("================================================================================");

for (const [id, data] of Object.entries(subjects)) {
  const avgScore = (data.scores.reduce((a,b)=>a+b,0)/data.scores.length).toFixed(1);
  const total = data.scores.length;
  console.log(`\n📌 Subjek: ${id} | Catatan Klinis: ${data.notes}`);
  console.log(`   Jumlah Sampel EEG Bersih: ${total}`);
  console.log(`   Rata-rata Skor Prediksi AI: ${avgScore} / 100`);
  console.log(`   Distribusi Prediksi Kelas : LOW: ${((data.classes.LOW/total)*100).toFixed(1)}% | ELEVATED: ${((data.classes.ELEVATED/total)*100).toFixed(1)}% | CRITICAL: ${((data.classes.CRITICAL/total)*100).toFixed(1)}%`);
}
