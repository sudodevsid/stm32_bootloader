const express = require('express');
const firmwareController = require('../controllers/firmwareController');

const router = express.Router();

router.post('/upload', firmwareController.upload.single('firmware'), firmwareController.uploadFirmware);
router.get('/latest', firmwareController.downloadLatest);

module.exports = router;
