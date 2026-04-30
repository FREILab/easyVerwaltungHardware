# Allows PlatformIO to upload directly to ElegantOTA
#
# To use:
# - copy this script into the same folder as your platformio.ini
# - set the following for your project in platformio.ini:
#
# extra_scripts = platformio_upload.py
# upload_protocol = custom
# custom_upload_url = <your upload URL>
# 
# An example of an upload URL:
#                custom_upload_url = http://192.168.1.123/update 
# also possible: custom_upload_url = http://domainname/update

import hashlib
import os
import re
import requests
import time
from urllib.parse import urlparse
from requests.auth import HTTPBasicAuth
Import("env")

try:
    from requests_toolbelt import MultipartEncoder, MultipartEncoderMonitor
    from tqdm import tqdm
except ImportError:
    env.Execute("$PYTHONEXE -m pip install requests_toolbelt")
    env.Execute("$PYTHONEXE -m pip install tqdm")
    from requests_toolbelt import MultipartEncoder, MultipartEncoderMonitor
    from tqdm import tqdm


def extract_define(content, define_name):
    pattern = rf'#define\s+{define_name}\s+["\']?([^"\'\n]+)["\']?(?:\s|$)'
    match = re.search(pattern, content)
    if not match:
        return None
    value = match.group(1).strip().strip('"').strip("'")
    return re.sub(r'\s*//.*$', '', value).strip()


def load_ota_credentials(env):
    secret_h_path = os.path.join(env.get("PROJECT_INCLUDE_DIR"), "secret.h")
    if os.path.isfile(secret_h_path):
        with open(secret_h_path, 'r') as secret_file:
            content = secret_file.read()
        username = extract_define(content, "OTA_USERNAME")
        password = extract_define(content, "OTA_PASSWORD")
        if username and password:
            return username, password, "secret.h"

    try:
        username = env.GetProjectOption('custom_username')
        password = env.GetProjectOption('custom_password')
        if username and password:
            return username, password, "platformio.ini"
    except:
        pass

    return None, None, None

def on_upload(source, target, env):
    firmware_path = str(source[0])

    auth = None
    upload_url_compatibility = env.GetProjectOption('custom_upload_url').rstrip('/')
    if upload_url_compatibility.endswith('/update'):
        upload_url = upload_url_compatibility[:-7]
    else:
        upload_url = upload_url_compatibility
    username, password, credential_source = load_ota_credentials(env)

    with open(firmware_path, 'rb') as firmware:
        md5 = hashlib.md5(firmware.read()).hexdigest()

        parsed_url = urlparse(upload_url)
        host_ip = parsed_url.netloc

        is_spiffs = source[0].name == "spiffs.bin"
        file_type = "fs" if is_spiffs else "fr"

        # execute GET request
        start_url = f"{upload_url}/ota/start?mode={file_type}&hash={md5}"

        start_headers = {
            'Host': host_ip,
            'User-Agent': 'Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:109.0) Gecko/20100101 Firefox/118.0',
            'Accept': '*/*',
            'Accept-Language': 'de,en-US;q=0.7,en;q=0.3',
            'Accept-Encoding': 'gzip, deflate',
            'Referer': f'{upload_url}/update',
            'Connection': 'keep-alive'
            }
        
        try:
            checkAuthResponse = requests.get(upload_url_compatibility, allow_redirects=False)
        except Exception as e:
            return 'Error checking auth: ' + repr(e)

        needs_auth = checkAuthResponse.status_code == 401

        if needs_auth:
            if username is None or password is None:
                print("No authentication values specified.")
                print('Please, add OTA_USERNAME and OTA_PASSWORD to include/secret.h.\n')
                return "Authentication required, but no credentials provided."
            print(f"Serverconfiguration: authentication needed ({credential_source}).")
            auth = HTTPBasicAuth(username, password)
            try:
                doUpdateAuth = requests.get(start_url, headers=start_headers, auth=auth)
            except Exception as e:
                return 'Error while authenticating: ' + repr(e)

            if doUpdateAuth.status_code != 200:
                return "Authentication failed " + str(doUpdateAuth.status_code)
            print("Authentication successful")
        else:
            auth = None
            print("Serverconfiguration: authentication not needed.")
            try:
                doUpdate = requests.get(start_url, headers=start_headers)
            except Exception as e:
                return 'Error while starting upload: ' + repr(e)

            if doUpdate.status_code == 401 and username is not None and password is not None:
                print(f"Serverconfiguration changed: retrying start request with authentication ({credential_source}).")
                auth = HTTPBasicAuth(username, password)
                try:
                    doUpdate = requests.get(start_url, headers=start_headers, auth=auth)
                except Exception as e:
                    return 'Error while authenticating after start failure: ' + repr(e)

            if doUpdate.status_code != 200:
                return "Start request failed " + str(doUpdate.status_code)

        firmware.seek(0)
        encoder = MultipartEncoder(fields={
            'MD5': md5,
            'firmware': ('firmware', firmware, 'application/octet-stream')}
        )

        bar = tqdm(desc='Upload Progress',
                   total=encoder.len,
                   dynamic_ncols=True,
                   unit='B',
                   unit_scale=True,
                   unit_divisor=1024
                   )

        monitor = MultipartEncoderMonitor(encoder, lambda monitor: bar.update(monitor.bytes_read - bar.n))

        post_headers = {
            'Host': host_ip,
            'User-Agent': 'Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:109.0) Gecko/20100101 Firefox/118.0',
            'Accept': '*/*',
            'Accept-Language': 'de,en-US;q=0.7,en;q=0.3',
            'Accept-Encoding': 'gzip, deflate',
            'Referer': f'{upload_url}/update',
            'Connection': 'keep-alive',
            'Content-Type': monitor.content_type,
            'Content-Length': str(monitor.len),
            'Origin': f'{upload_url}'
        }

        try:
            response = requests.post(f"{upload_url}/ota/upload", data=monitor, headers=post_headers, auth=auth)
        except Exception as e:
            return 'Error while uploading: ' + repr(e)
        
        bar.close()
        time.sleep(0.1)
        
        if response.status_code != 200:
            message = "\nUpload failed.\nServer response: " + response.text
            tqdm.write(message)
        else:
            message = "\nUpload successful.\nServer response: " + response.text
            tqdm.write(message)

            
env.Replace(UPLOADCMD=on_upload)