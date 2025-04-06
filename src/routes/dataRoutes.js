// routes/dataRoutes.js
const express = require('express');
const router = express.Router();
const dataModel = require('../models/dataModel');
const { error } = require('console');

router.post('/register', async (req, res) => {
  const {mac} = req.body;

  if (!mac) {
    return res.status(400).send('Missing id.');
  }

  try {
    dataModel.checkRefreshToken({id: mac}, async (err, result) => {
      if (err) {
        console.error('Error checking refresh token:', err);
        return res.status(500).send('Database error.');
      }

      const hasToken = result.rows.length > 0 && result.rows[0].refresh_token !== null;

      if (hasToken) {
        console.log('User already exists with token.');
        return res.redirect(`/success.html`);
      }

      const result = await dataModel.registerUser({ id: mac });

      if (!result.success && result.message === 'User already exists') {
        console.log('User already exists:', mac);
      } else {
        console.log('New user registered:', mac);
      }

      return res.redirect(`/spotify/login?id=${mac}`); 
    });

  } catch (err) {
    console.error('Error in register route:', err);
    res.status(500).send('Server error.');
  }
});

// Retrieve refresh token by device UUID
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
