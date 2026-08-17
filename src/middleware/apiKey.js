function requireApiKey(req, res, next) {
  const key = req.header('x-api-key');
  if (!process.env.UPLOAD_API_KEY || key !== process.env.UPLOAD_API_KEY) {
    return res.status(401).json({ error: 'invalid or missing x-api-key header' });
  }
  next();
}

module.exports = requireApiKey;
