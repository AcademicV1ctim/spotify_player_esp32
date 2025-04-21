const supabase = require('../services/supabaseClient');

module.exports = {
    checkRefreshToken: async ({ id }) => {
      const { data, error } = await supabase
        .from('setup')
        .select('refresh_token')
        .eq('id', id)
        .single();
  
      if (error) {
        throw { code: 500, message: error.message };
      }
  
      if (!data || !data.refresh_token) {
        throw { code: 404, message: 'Refresh token not found' };
      }
  
      return data;
    }
  };
  