const express = require('express');
const router = express.Router();
const dataModel = require('../models/dataModel');

router.get('/retrieve', async (req, res) => {
  const { id: mac } = req.query;

  if (!mac) {
    return res.status(400).json({ error: 'Missing id parameter' });
  }

  try {
    const result = await dataModel.checkRefreshToken({ id: mac });
    res.status(200).json({ refresh_token: result.refresh_token });
  } catch (err) {
    console.error('Supabase error:', err);
    res.status(err.code || 500).json({ error: err.message || 'Internal server error' });
  }  
});

module.exports = router;
