import hashlib
import os
import json
import shutil

ZERO_MD5 = '00000000000000000000000000000000'

version_property_prefixes = ['firmware', 'deviceProtocol', 'moduleProtocol', 'userConfig', 'hardwareConfig', 'smartMacros']
patch_versions = ['Major', 'Minor', 'Patch']

def generate_versions(package_json, git_info, use_real_shas, use_zero_versions):
    package_json = json.loads(json.dumps(package_json))  # Deep copy

    if use_zero_versions:
        git_info = {
            'repo': '',
            'tag': ''
        }

    version_variables = '\n'.join([
        f"const version_t {prefix}Version = {{ {', '.join(['0' if use_zero_versions else package_json[f'{prefix}Version'].split('.')[i] for i in range(len(patch_versions))])} }};"
        for prefix in version_property_prefixes
    ])

    device_md5_sums = '\n'.join([
        f'    [{device["deviceId"]}] = "{ZERO_MD5 if not use_real_shas else calculate_md5_checksum_of_file(os.path.join(os.path.dirname(__file__), "..", device["source"]))}",'
        for device in package_json['devices']
    ])

    module_md5_sums = '\n'.join([
        f'    [{module["moduleId"]}] = "{ZERO_MD5 if not use_real_shas else calculate_md5_checksum_of_file(os.path.join(os.path.dirname(__file__), "..", module["source"]))}",'
        for module in package_json['modules']
    ])

    with open(os.path.join(os.path.dirname(__file__), '..', 'shared', 'versions.c'), 'w') as f:
        f.write(f"""// Please do not edit this file by hand!
// It is to be regenerated by /scripts/generate_versions.py
#include "versioning.h"

{version_variables}

const char gitRepo[] = "{git_info['repo']}";
const char gitTag[] = "{git_info['tag']}";

#ifdef DEVICE_COUNT
const char *const DeviceMD5Checksums[DEVICE_COUNT + 1] = {{
{device_md5_sums}
}};
#endif

const char *const ModuleMD5Checksums[ModuleId_AllCount] = {{
{module_md5_sums}
}};
""")

    return {
        'devices': package_json['devices'],
        'modules': package_json['modules']
    }

def calculate_md5_checksum_of_file(file_path):
    with open(file_path, 'rb') as f:
        file_data = f.read()
    return hashlib.md5(file_data).hexdigest()
