const express = require('express');
const requireApiKey = require('../middleware/apiKey');
const firmwareController = require('../controllers/firmwareController');

const router = express.Router();

router.post(
  '/upload',
  requireApiKey,
  firmwareController.upload.single('firmware'),
  firmwareController.uploadFirmware
);
router.get('/latest/info', firmwareController.getLatestInfo);
router.get('/latest', firmwareController.downloadLatest);

module.exports = router;
