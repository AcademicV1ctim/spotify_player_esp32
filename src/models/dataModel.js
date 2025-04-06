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

// Check if user exists and return refresh token (if any)
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
  