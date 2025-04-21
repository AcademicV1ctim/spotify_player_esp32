// routes/dataRoutes.js
const express = require('express');
const router = express.Router();
const dataModel = require('../models/dataModel');

router.get('/retrieve', async (req, res) => {
  const {id: mac} = req.query; // ESP is sending MAC address as id

  if (!mac) {
    return res.status(400).json({ error: 'Missing id parameter' });
  }

  dataModel.checkRefreshToken({id: mac}, (err, result) => {
    if (err) {
      console.error('Database error:', err);
      return res.status(500).json({ error: 'Database error' });
    }

    if (result.rows.length === 0 || !result.rows[0].refresh_token || result.rows[0].refresh_token == null) {
      return res.status(404).json({ error: 'Refresh token not found' });
    }

    res.status(200).json({ refresh_token: result.rows[0].refresh_token });
  });
});

module.exports = router;
