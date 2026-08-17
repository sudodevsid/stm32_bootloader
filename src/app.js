require('dotenv').config();
const express = require('express');
const firmwareRoutes = require('./routes/firmware');

const app = express();

app.get('/health', (req, res) => res.json({ status: 'ok' }));
app.use('/api/firmware', firmwareRoutes);

const PORT = process.env.PORT || 3000;

app.listen(PORT, () => {
  console.log(`firmware server listening on port ${PORT}`);
});
