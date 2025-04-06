// routes/dataRoutes.js
const express = require('express');
const { v5: uuidv5 } = require('uuid');
const router = express.Router();
const dataModel = require('../models/dataModel');
const { error } = require('console');

const NAMESPACE = process.env.UUID_NAMESPACE; 

router.post('/register', async (req, res) => {
  const { mac } = req.body;

  if (!mac) {
    return res.status(400).send('Missing MAC address.');
  }

  try {
    const deviceId = uuidv5(mac, NAMESPACE);
    console.log('Generated UUID:', deviceId);

    dataModel.checkRefreshToken({ id: deviceId }, async (err, result) => {
      if (err) {
        console.error('Error checking refresh token:', err);
        return res.status(500).send('Database error.');
      }

      const hasToken = result.rows.length > 0 && result.rows[0].refresh_token !== null;

      if (hasToken) {
        console.log('User already exists with token.');
        return res.redirect(`/success.html`);
      }

      // 2. If not found, register new user
      await dataModel.registerUser({ id: deviceId });

      console.log('New user registered:', deviceId);
      return res.redirect(`/spotify/login?id=${deviceId}`); 
    });

  } catch (err) {
    console.error('Error in register route:', err);
    res.status(500).send('Server error.');
  }
});

// Retrieve refresh token by device UUID
router.get('/retrieve', async (req, res) => {
  const { id: mac } = req.query; // ESP is sending MAC address as id

  if (!mac) {
    return res.status(400).json({ error: 'Missing id parameter' });
  }

  // Convert MAC to UUIDv5
  const deviceId = uuidv5(mac, NAMESPACE);
  console.log('Converted UUID:', deviceId);

  dataModel.checkRefreshToken({ id: deviceId }, (err, result) => {
    if (err) {
      console.error('Database error:', err);
      return res.status(500).json({ error: 'Database error' });
    }

    if (result.rows.length === 0 || !result.rows[0].refresh_token) {
      return res.status(404).json({ error: 'Refresh token not found' });
    }

    res.status(200).json({ refresh_token: result.rows[0].refresh_token });
  });
});

module.exports = router;
