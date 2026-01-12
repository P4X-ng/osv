from osv.modules import api

# This module is deprecated. Use java-base instead.
# Kept for backward compatibility.
api.require('java-base')
provides = ['java-cmd']
