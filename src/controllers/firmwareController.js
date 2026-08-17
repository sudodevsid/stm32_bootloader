const multer = require('multer');
const fs = require('fs');
const path = require('path');

const FIRMWARE_DIR = path.join(__dirname, '..', '..', 'firmware');
const LATEST_BIN_PATH = path.join(FIRMWARE_DIR, 'latest.bin');

fs.mkdirSync(FIRMWARE_DIR, { recursive: true });

const storage = multer.diskStorage({
  destination: FIRMWARE_DIR,
  filename: (req, file, cb) => cb(null, 'latest.bin'),
});

const upload = multer({ storage });

function uploadFirmware(req, res) {
  if (!req.file) {
    return res.status(400).json({ error: 'multipart field "firmware" with the .bin file is required' });
  }
  res.status(201).json({ message: 'firmware uploaded', size: req.file.size });
}

function downloadLatest(req, res) {
  if (!fs.existsSync(LATEST_BIN_PATH)) {
    return res.status(404).json({ error: 'no firmware uploaded yet' });
  }
  res.set('Content-Type', 'application/octet-stream');
  res.download(LATEST_BIN_PATH, 'firmware.bin');
}

module.exports = {
  upload,
  uploadFirmware,
  downloadLatest,
};
