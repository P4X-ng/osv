from osv.modules import api

api.require('ca-certificates')
api.require('libz')
provides = ['java-cmd']
