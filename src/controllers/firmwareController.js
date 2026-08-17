const multer = require('multer');
const fs = require('fs');
const path = require('path');

const FIRMWARE_DIR = path.join(__dirname, '..', '..', 'firmware');
const LATEST_NAME_PATTERN = /^latest\./;

fs.mkdirSync(FIRMWARE_DIR, { recursive: true });

function findLatestFile() {
  const match = fs.readdirSync(FIRMWARE_DIR).find((name) => LATEST_NAME_PATTERN.test(name));
  return match ? path.join(FIRMWARE_DIR, match) : null;
}

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

function uploadFirmware(req, res) {
  if (!req.file) {
    return res.status(400).json({ error: 'multipart field "firmware" with the .bin or .hex file is required' });
  }
  res.status(201).json({ message: 'firmware uploaded', filename: req.file.filename, size: req.file.size });
}

function downloadLatest(req, res) {
  const filePath = findLatestFile();
  if (!filePath) {
    return res.status(404).json({ error: 'no firmware uploaded yet' });
  }
  res.set('Content-Type', 'application/octet-stream');
  res.download(filePath, path.basename(filePath));
}

module.exports = {
  upload,
  uploadFirmware,
  downloadLatest,
};
