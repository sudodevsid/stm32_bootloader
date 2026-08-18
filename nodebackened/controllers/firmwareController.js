const multer = require('multer');
const fs = require('fs');
const path = require('path');
const { hexToBin } = require('../utils/intelHex');
const { crc32 } = require('../utils/crc32');

const FIRMWARE_DIR = path.join(__dirname, '..', '..', 'firmware');
const LATEST_NAME_PATTERN = /^latest\./;
const APP_BIN_PATH = path.join(FIRMWARE_DIR, 'app.bin');
const MANIFEST_PATH = path.join(FIRMWARE_DIR, 'manifest.txt');
const OTA_BIN_URL_PATH = '/firmware/stm32f446/app.bin';

fs.mkdirSync(FIRMWARE_DIR, { recursive: true });

const storage = multer.diskStorage({
  destination: FIRMWARE_DIR,
  filename: (req, file, cb) => {
    for (const name of fs.readdirSync(FIRMWARE_DIR)) {
      if (LATEST_NAME_PATTERN.test(name)) {
        fs.unlinkSync(path.join(FIRMWARE_DIR, name));
      }
    }
    const ext = path.extname(file.originalname) || '.bin';
    cb(null, `latest${ext}`);
  },
});

const upload = multer({ storage });

function nextVersion() {
  if (!fs.existsSync(MANIFEST_PATH)) return 1;
  const match = fs.readFileSync(MANIFEST_PATH, 'utf8').match(/^version=(\d+)/m);
  return match ? Number(match[1]) + 1 : 1;
}

function writeOtaArtifacts(sourceFilePath) {
  const raw = fs.readFileSync(sourceFilePath);
  const binData = path.extname(sourceFilePath).toLowerCase() === '.hex' ? hexToBin(raw) : raw;

  const manifest = [
    `version=${nextVersion()}`,
    `size=${binData.length}`,
    `crc32=${crc32(binData).toString(16).toUpperCase().padStart(8, '0')}`,
    `path=${OTA_BIN_URL_PATH}`,
    '',
  ].join('\n');

  fs.writeFileSync(APP_BIN_PATH, binData);
  fs.writeFileSync(MANIFEST_PATH, manifest);
}

function uploadFirmware(req, res) {
  if (!req.file) {
    return res.status(400).json({ error: 'multipart field "firmware" with the .bin or .hex file is required' });
  }
  writeOtaArtifacts(req.file.path);
  res.status(201).json({ message: 'firmware uploaded', filename: req.file.filename, size: req.file.size });
}

function downloadLatest(req, res) {
  if (!fs.existsSync(APP_BIN_PATH)) {
    return res.status(404).json({ error: 'no firmware uploaded yet' });
  }
  res.set('Content-Type', 'application/octet-stream');
  res.download(APP_BIN_PATH, 'latest.bin');
}

function getManifest(req, res) {
  if (!fs.existsSync(MANIFEST_PATH)) {
    return res.status(404).send('no firmware uploaded yet');
  }
  res.set('Content-Type', 'text/plain');
  res.sendFile(MANIFEST_PATH);
}

function getAppBin(req, res) {
  if (!fs.existsSync(APP_BIN_PATH)) {
    return res.status(404).json({ error: 'no firmware uploaded yet' });
  }
  res.set('Content-Type', 'application/octet-stream');
  res.download(APP_BIN_PATH, 'app.bin');
}

module.exports = {
  upload,
  uploadFirmware,
  downloadLatest,
  getManifest,
  getAppBin,
};
