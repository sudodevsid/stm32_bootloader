const multer = require('multer');
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

const FIRMWARE_DIR = path.join(__dirname, '..', '..', 'firmware');
const LATEST_BIN_PATH = path.join(FIRMWARE_DIR, 'latest.bin');
const METADATA_PATH = path.join(FIRMWARE_DIR, 'metadata.json');

const upload = multer({ dest: path.join(FIRMWARE_DIR, 'tmp') });

function readMetadata() {
  if (!fs.existsSync(METADATA_PATH)) return null;
  return JSON.parse(fs.readFileSync(METADATA_PATH, 'utf8'));
}

function sha256File(filePath) {
  const hash = crypto.createHash('sha256');
  hash.update(fs.readFileSync(filePath));
  return hash.digest('hex');
}

function uploadFirmware(req, res) {
  if (!req.file) {
    return res.status(400).json({ error: 'multipart field "firmware" with the .bin file is required' });
  }

  fs.mkdirSync(FIRMWARE_DIR, { recursive: true });
  fs.copyFileSync(req.file.path, LATEST_BIN_PATH);
  fs.unlinkSync(req.file.path);

  const metadata = {
    version: req.body.version || String(Date.now()),
    originalName: req.file.originalname,
    size: fs.statSync(LATEST_BIN_PATH).size,
    sha256: sha256File(LATEST_BIN_PATH),
    uploadedAt: new Date().toISOString(),
  };
  fs.writeFileSync(METADATA_PATH, JSON.stringify(metadata, null, 2));

  res.status(201).json({ message: 'firmware uploaded', metadata });
}

function getLatestInfo(req, res) {
  const metadata = readMetadata();
  if (!metadata) return res.status(404).json({ error: 'no firmware uploaded yet' });
  res.json(metadata);
}

function downloadLatest(req, res) {
  if (!fs.existsSync(LATEST_BIN_PATH)) {
    return res.status(404).json({ error: 'no firmware uploaded yet' });
  }
  const metadata = readMetadata();
  if (metadata) {
    res.set('X-Firmware-Version', metadata.version);
    res.set('X-Firmware-Sha256', metadata.sha256);
  }
  res.set('Content-Type', 'application/octet-stream');
  res.download(LATEST_BIN_PATH, 'firmware.bin');
}

module.exports = {
  upload,
  uploadFirmware,
  getLatestInfo,
  downloadLatest,
};
