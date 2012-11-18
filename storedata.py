import re
import sys
import httplib2
import time

from apiclient.discovery import build
from oauth2client.file import Storage
from oauth2client.client import AccessTokenRefreshError
from datetime import datetime

inT = inRh = outT = outRh = None
storage = Storage('/home/pi/homectrl/cgi/credfile')
credentials = storage.get()

while True:
  with open('/run/shm/w.dat') as f:
    for line in f:
      m = re.match(r".Sensor(.) T: (.*) Rh: (.*)%", line)
      if m != None:
        t = m.groups()
        if t[0] == '0':
            inT, inRh = t[1:]
        elif t[0] == '1':
            outT, outRh = t[1:]

  if credentials is None or credentials.invalid:
    print 'credentials not OK'
    sys.exit(1)

  http = httplib2.Http()
  http = credentials.authorize(http)
  service = build('fusiontables', 'v1', http=http)
  dt = datetime.now().strftime('%Y.%m.%d %H:%M:%S')
  sql = "INSERT INTO 1acXoZPFQYzgi9B-NwuuqZFHpQnAj36NYIA6XvEg (Date, 'Indoor Temp', 'Indoor Humidity', 'Outdoor Temp', 'Outdoor Humidity') VALUES ('{}',{},{},{},{});".format(dt, inT, inRh, outT, outRh)
#  print sql
  service.query().sql(sql=sql).execute()
  time.sleep(300)

