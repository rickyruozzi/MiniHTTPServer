import json
import urllib.request
import urllib.error

url = 'http://127.0.0.1:8080/'
body = json.dumps({"message": "test post request"}).encode('utf-8')

req = urllib.request.Request(url, data=body, method='POST')
req.add_header('Content-Type', 'application/json')

try:
    with urllib.request.urlopen(req, timeout=5) as response:
        print('Status:', response.status)
        print('Content-Type:', response.headers.get('Content-Type'))
        print('Body:', response.read().decode('utf-8'))
except urllib.error.HTTPError as e:
    print('Status:', e.code)
    print('Content-Type:', e.headers.get('Content-Type'))
    print('Body:', e.read().decode('utf-8'))
except Exception as e:
    print('Errore di connessione:', e)
