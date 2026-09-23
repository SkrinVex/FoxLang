#!/usr/bin/env python3
import os
import sys
import json
import zipfile

def build_vsix(source_dir, output_vsix, version):
    pkg_json_path = os.path.join(source_dir, 'package.json')
    if not os.path.exists(pkg_json_path):
        raise FileNotFoundError(f"Cannot find {pkg_json_path}")

    with open(pkg_json_path, 'r', encoding='utf-8') as f:
        pkg = json.load(f)

    pkg_version = pkg.get('version', version)
    pkg_id = pkg.get('name', 'foxlang-language')
    pkg_publisher = pkg.get('publisher', 'SkrinVex')
    display_name = pkg.get('displayName', 'FoxLang')
    description = pkg.get('description', 'FoxLang Language Support')

    content_types = '''<?xml version="1.0" encoding="utf-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="json" ContentType="application/json"/>
  <Default Extension="js" ContentType="application/javascript"/>
  <Default Extension="md" ContentType="text/markdown"/>
  <Default Extension="vsixmanifest" ContentType="text/xml"/>
</Types>'''

    vsix_manifest = f'''<?xml version="1.0" encoding="utf-8"?>
<PackageManifest Version="2.0.0" xmlns="http://schemas.microsoft.com/developer/vsx-schema/2011" xmlns:d="http://schemas.microsoft.com/developer/vsx-schema-design/2011">
  <Metadata>
    <Identity Id="{pkg_id}" Version="{pkg_version}" Language="en-US" Publisher="{pkg_publisher}"/>
    <DisplayName>{display_name}</DisplayName>
    <Description xml:space="preserve">{description}</Description>
    <Tags>foxlang,fox,lsp,language server</Tags>
    <Categories>Programming Languages</Categories>
  </Metadata>
  <Installation>
    <InstallationTarget Id="Microsoft.VisualStudio.Code"/>
  </Installation>
  <Dependencies/>
  <Assets>
    <Asset Type="Microsoft.VisualStudio.Code.Manifest" Path="extension/package.json" Addressable="true"/>
  </Assets>
</PackageManifest>'''

    os.makedirs(os.path.dirname(os.path.abspath(output_vsix)), exist_ok=True)
    with zipfile.ZipFile(output_vsix, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('[Content_Types].xml', content_types)
        z.writestr('extension.vsixmanifest', vsix_manifest)
        for root, dirs, files in os.walk(source_dir):
            for file in files:
                full_path = os.path.join(root, file)
                rel_path = os.path.relpath(full_path, source_dir)
                z.write(full_path, f"extension/{rel_path}")

    print(f"Successfully packaged VSIX: {output_vsix} (v{pkg_version})")

if __name__ == '__main__':
    src = sys.argv[1] if len(sys.argv) > 1 else 'editors/vscode'
    ver = sys.argv[2] if len(sys.argv) > 2 else '5.5.3'
    out = sys.argv[3] if len(sys.argv) > 3 else f'foxlang-{ver}.vsix'
    build_vsix(src, out, ver)
