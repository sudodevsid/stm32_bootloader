require('dotenv').config();
const express = require('express');
const firmwareRoutes = require('./routes/firmware');
const firmwareController = require('./controllers/firmwareController');

const app = express();

app.get('/health', (req, res) => res.json({ status: 'ok' }));
app.use('/api/firmware', firmwareRoutes);

// Matches BL_HTTP_MANIFEST_PATH in the STM32 bootloader's bootloader_config.h
app.get('/firmware/stm32f446/manifest.txt', firmwareController.getManifest);
app.get('/firmware/stm32f446/app.bin', firmwareController.getAppBin);

const PORT = process.env.PORT || 3000;

app.listen(PORT, () => {
  console.log(`firmware server listening on port ${PORT}`);
});
