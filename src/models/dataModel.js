const pool = require('../services/db.js');

// Register user if not exists
module.exports.registerUser = async (data) => {
    const SQLSTATEMENT = `
        INSERT INTO "Users" (id)
        VALUES ($1)
        ON CONFLICT (id) DO NOTHING;
    `;
    const VALUES = [data.id];
    return pool.query(SQLSTATEMENT, VALUES);
};

// Check if refresh token exists for a user
module.exports.checkRefreshToken = async (data, callback) => {
    const SQLSTATEMENT = `
        SELECT refresh_token 
        FROM "Users" 
        WHERE id = $1;
    `;
    const VALUES = [data.id];

    try {
        const result = await pool.query(SQLSTATEMENT, VALUES);
        callback(null, result);
    } catch (error) {
        callback(error);
    }
};

module.exports.updateRefreshToken = async (data) => {
    const SQLSTATEMENT = `
      UPDATE "Users"
      SET refresh_token = $1
      WHERE id = $2;
    `;
  
    const VALUES = [data.refresh_token, data.id];
  
    try {
      await pool.query(SQLSTATEMENT, VALUES);
      console.log(`Refresh token updated for device: ${data.id}`);
    } catch (error) {
      console.error('Error updating refresh token:', error);
      throw error;
    }
  };