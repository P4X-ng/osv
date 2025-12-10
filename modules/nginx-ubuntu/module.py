from osv.modules import api

# Create nginx configuration directory structure
api.run_on_init('/usr/bin/mkdir -p /etc/nginx/sites-enabled')
api.run_on_init('/usr/bin/mkdir -p /var/log/nginx')
api.run_on_init('/usr/bin/mkdir -p /var/lib/nginx')

# Default nginx application
default = api.run('/usr/sbin/nginx -g "daemon off;"')