#!/usr/bin/python3

# this script will create/update info plist files based on config.h and copy resources to the ~/Music/PLUG_NAME folder or the bundle depending on PLUG_SHARED_RESOURCES

kAudioUnitType_MusicDevice      = "aumu"
kAudioUnitType_MusicEffect      = "aumf"
kAudioUnitType_Effect           = "aufx"
kAudioUnitType_MIDIProcessor    = "aumi"

DONT_COPY = ("")

import plistlib, os, datetime, fileinput, glob, sys, string, shutil, tempfile

scriptpath = os.path.dirname(os.path.realpath(__file__))
projectpath = os.path.abspath(os.path.join(scriptpath, os.pardir))

IPLUG2_ROOT = "../../iPlug2"

sys.path.insert(0, os.path.join(os.getcwd(), IPLUG2_ROOT + '/Scripts'))

from parse_config import parse_config, parse_xcconfig

def copy_resources_to_destination(projectpath, dst, label=""):
  """Copy image and font resources from project to destination folder."""
  display_dst = label if label else dst

  if os.path.exists(projectpath + "/resources/img/"):
    for img in os.listdir(projectpath + "/resources/img/"):
      print("copying " + img + " to " + display_dst)
      shutil.copy(projectpath + "/resources/img/" + img, dst)

  if os.path.exists(projectpath + "/resources/fonts/"):
    for font in os.listdir(projectpath + "/resources/fonts/"):
      print("copying " + font + " to " + display_dst)
      shutil.copy(projectpath + "/resources/fonts/" + font, dst)

  if os.path.exists(projectpath + "/factory_patches/"):
    factory_patches_dst = os.path.join(dst, "factory_patches")
    print("copying factory patches to " + display_dst)
    if os.path.isdir(factory_patches_dst):
      shutil.rmtree(factory_patches_dst)
    elif os.path.exists(factory_patches_dst):
      os.remove(factory_patches_dst)
    shutil.copytree(projectpath + "/factory_patches/", factory_patches_dst)

def load_plist(plistpath):
  with open(plistpath, 'rb') as fp:
    return plistlib.load(fp)

def dump_plist_atomic(plistpath, plist):
  # Multiple macOS targets rewrite these shared plists in parallel during
  # aggregate builds, so replace them atomically to avoid readers seeing a
  # truncated file between open(..., 'wb') and plistlib.dump().
  plist_bytes = plistlib.dumps(plist)

  try:
    with open(plistpath, 'rb') as fp:
      if fp.read() == plist_bytes:
        return
    file_mode = os.stat(plistpath).st_mode & 0o777
  except FileNotFoundError:
    file_mode = 0o644

  fd, tmppath = tempfile.mkstemp(
    prefix=os.path.basename(plistpath) + ".",
    suffix=".tmp",
    dir=os.path.dirname(plistpath))
  try:
    with os.fdopen(fd, 'wb') as fp:
      fp.write(plist_bytes)
    os.chmod(tmppath, file_mode)
    os.replace(tmppath, plistpath)
  except:
    if os.path.exists(tmppath):
      os.remove(tmppath)
    raise

def demo_build_enabled():
  return os.environ.get('ADDIVOX_DEMO', '').lower() in ('1', 'yes', 'true', 'on')

def objc_prefix():
  # Xcode exports build settings to run-script phases, so this tracks the
  # ADDIVOX_OBJC_PREFIX the Objective-C classes are actually compiled with.
  return os.environ.get('ADDIVOX_OBJC_PREFIX', 'vAddivox')

def apply_demo_config(config):
  if demo_build_enabled():
    config['PLUG_NAME'] = 'AddivoxDemo'
    config['BUNDLE_NAME'] = 'AddivoxDemo'
    config['PLUG_UNIQUE_ID'] = 'AdvD'
    config['AUV2_VIEW_CLASS'] = 'AddivoxDemo_View'
    config['AUV2_VIEW_CLASS_STR'] = 'AddivoxDemo_View'

def main():
  config = parse_config(projectpath)
  resource_bundle_name = config['BUNDLE_NAME']
  apply_demo_config(config)
  xcconfig = parse_xcconfig(os.path.join(os.getcwd(), IPLUG2_ROOT +  '/common-mac.xcconfig'))

  CFBundleGetInfoString = config['BUNDLE_NAME'] + " v" + config['FULL_VER_STR'] + " " + config['PLUG_COPYRIGHT_STR']
  CFBundleVersion = config['FULL_VER_STR']
  CFBundlePackageType = "BNDL"
  CSResourcesFileMapped = True
  LSMinimumSystemVersion = xcconfig['DEPLOYMENT_TARGET']

  print("Copying resources ...")

  if config['PLUG_SHARED_RESOURCES']:
    dst = os.path.expanduser("~") + "/Music/" + config['BUNDLE_NAME'] + "/Resources"
  else:
    dst = os.path.join(os.environ["TARGET_BUILD_DIR"], os.environ["UNLOCALIZED_RESOURCES_FOLDER_PATH"].lstrip('/'))

  if os.path.exists(dst) == False:
    os.makedirs(dst + "/", 0o0755 )

  copy_resources_to_destination(projectpath, dst)

  # Also copy resources to AUv3 Framework for macOS sandbox compatibility
  # (AUv3 appex cannot access container app's resources in sandbox)
  if not config['PLUG_SHARED_RESOURCES']:
    target_build_dir = os.environ.get("TARGET_BUILD_DIR", "")
    if target_build_dir:
      framework_dst = os.path.join(target_build_dir, config['BUNDLE_NAME'] + ".app/Contents/Frameworks/AUv3Framework.framework/Versions/A/Resources")

      if os.path.exists(os.path.dirname(framework_dst)):
        if not os.path.exists(framework_dst):
          os.makedirs(framework_dst, 0o0755)

        copy_resources_to_destination(projectpath, framework_dst, "AUv3 Framework")

  print("Processing Info.plist files...")

# VST3

  plistpath = projectpath + "/resources/" + resource_bundle_name + "-VST3-Info.plist"
  vst3 = load_plist(plistpath)
  vst3['CFBundleExecutable'] = config['BUNDLE_NAME']
  vst3['CFBundleGetInfoString'] = CFBundleGetInfoString
  vst3['CFBundleIdentifier'] = config['BUNDLE_DOMAIN'] + "." + config['BUNDLE_MFR'] + ".vst3." + config['BUNDLE_NAME'] + ""
  vst3['CFBundleName'] = config['BUNDLE_NAME']
  vst3['CFBundleVersion'] = CFBundleVersion
  vst3['CFBundleShortVersionString'] = CFBundleVersion
  vst3['LSMinimumSystemVersion'] = LSMinimumSystemVersion
  vst3['CFBundlePackageType'] = CFBundlePackageType
  vst3['CFBundleSignature'] = config['PLUG_UNIQUE_ID']
  vst3['CSResourcesFileMapped'] = CSResourcesFileMapped

  dump_plist_atomic(plistpath, vst3)
# VST2

  plistpath = projectpath + "/resources/" + resource_bundle_name + "-VST2-Info.plist"
  vst2 = load_plist(plistpath)
  vst2['CFBundleExecutable'] = config['BUNDLE_NAME']
  vst2['CFBundleGetInfoString'] = CFBundleGetInfoString
  vst2['CFBundleIdentifier'] = config['BUNDLE_DOMAIN'] + "." + config['BUNDLE_MFR'] + ".vst." + config['BUNDLE_NAME'] + ""
  vst2['CFBundleName'] = config['BUNDLE_NAME']
  vst2['CFBundleVersion'] = CFBundleVersion
  vst2['CFBundleShortVersionString'] = CFBundleVersion
  vst2['LSMinimumSystemVersion'] = LSMinimumSystemVersion
  vst2['CFBundlePackageType'] = CFBundlePackageType
  vst2['CFBundleSignature'] = config['PLUG_UNIQUE_ID']
  vst2['CSResourcesFileMapped'] = CSResourcesFileMapped

  dump_plist_atomic(plistpath, vst2)

# CLAP

  plistpath = projectpath + "/resources/" + resource_bundle_name + "-CLAP-Info.plist"
  clap = load_plist(plistpath)
  clap['CFBundleExecutable'] = config['BUNDLE_NAME']
  clap['CFBundleGetInfoString'] = CFBundleGetInfoString
  clap['CFBundleIdentifier'] = config['BUNDLE_DOMAIN'] + "." + config['BUNDLE_MFR'] + ".clap." + config['BUNDLE_NAME'] + ""
  clap['CFBundleName'] = config['BUNDLE_NAME']
  clap['CFBundleVersion'] = CFBundleVersion
  clap['CFBundleShortVersionString'] = CFBundleVersion
  clap['LSMinimumSystemVersion'] = LSMinimumSystemVersion
  clap['CFBundlePackageType'] = CFBundlePackageType
  clap['CFBundleSignature'] = config['PLUG_UNIQUE_ID']
  clap['CSResourcesFileMapped'] = CSResourcesFileMapped

  dump_plist_atomic(plistpath, clap)

# AUDIOUNIT v2

  plistpath = projectpath + "/resources/" + resource_bundle_name + "-AU-Info.plist"
  auv2 = load_plist(plistpath)
  auv2['CFBundleExecutable'] = config['BUNDLE_NAME']
  auv2['CFBundleGetInfoString'] = CFBundleGetInfoString
  auv2['CFBundleIdentifier'] = config['BUNDLE_DOMAIN'] + "." + config['BUNDLE_MFR'] + ".audiounit." + config['BUNDLE_NAME'] + ""
  auv2['CFBundleName'] = config['BUNDLE_NAME']
  auv2['CFBundleVersion'] = CFBundleVersion
  auv2['CFBundleShortVersionString'] = CFBundleVersion
  auv2['LSMinimumSystemVersion'] = LSMinimumSystemVersion
  auv2['CFBundlePackageType'] = CFBundlePackageType
  auv2['CFBundleSignature'] = config['PLUG_UNIQUE_ID']
  auv2['CSResourcesFileMapped'] = CSResourcesFileMapped
  auv2['NSPrincipalClass'] = config['AUV2_VIEW_CLASS_STR']

  if config['PLUG_TYPE'] == 0:
    if config['PLUG_DOES_MIDI_IN']:
      COMPONENT_TYPE = kAudioUnitType_MusicEffect
    else:
      COMPONENT_TYPE = kAudioUnitType_Effect
  elif config['PLUG_TYPE'] == 1:
    COMPONENT_TYPE = kAudioUnitType_MusicDevice
  elif config['PLUG_TYPE'] == 2:
    COMPONENT_TYPE = kAudioUnitType_MIDIProcessor

  auv2['AudioUnit Version'] = config['PLUG_VERSION_HEX']
  auv2['AudioComponents'] = [{}]
  auv2['AudioComponents'][0]['description'] = config['PLUG_NAME']
  auv2['AudioComponents'][0]['factoryFunction'] = config['AUV2_FACTORY']
  auv2['AudioComponents'][0]['manufacturer'] = config['PLUG_MFR_ID']
  auv2['AudioComponents'][0]['name'] = config['PLUG_MFR'] + ": " + config['PLUG_NAME']
  auv2['AudioComponents'][0]['subtype'] = config['PLUG_UNIQUE_ID']
  auv2['AudioComponents'][0]['type'] = COMPONENT_TYPE
  auv2['AudioComponents'][0]['version'] = config['PLUG_VERSION_INT']
  auv2['AudioComponents'][0]['sandboxSafe'] = True

  dump_plist_atomic(plistpath, auv2)
# AUDIOUNIT v3

  if config['PLUG_HAS_UI']:
    NSEXTENSIONPOINTIDENTIFIER  = "com.apple.AudioUnit-UI"
  else:
    NSEXTENSIONPOINTIDENTIFIER  = "com.apple.AudioUnit"

  plistpath = projectpath + "/resources/" + resource_bundle_name + "-macOS-AUv3-Info.plist"
  auv3 = load_plist(plistpath)
  auv3['CFBundleExecutable'] = config['BUNDLE_NAME']
  auv3['CFBundleGetInfoString'] = CFBundleGetInfoString
  auv3['CFBundleIdentifier'] = config['BUNDLE_DOMAIN'] + "." + config['BUNDLE_MFR'] + ".app." + config['BUNDLE_NAME'] + ".AUv3"
  auv3['CFBundleName'] = config['BUNDLE_NAME']
  auv3['CFBundleVersion'] = CFBundleVersion
  auv3['CFBundleShortVersionString'] = CFBundleVersion
  auv3['LSMinimumSystemVersion'] = "10.13.0"
  auv3['CFBundlePackageType'] = "XPC!"
  auv3['NSExtension'] = dict(
  NSExtensionAttributes = dict(
                               AudioComponentBundle = "com.rrwick.app." + config['BUNDLE_NAME'] + ".AUv3Framework",
                               AudioComponents = [{}]),
#                               NSExtensionServiceRoleType = "NSExtensionServiceRoleTypeEditor",
  NSExtensionPointIdentifier = NSEXTENSIONPOINTIDENTIFIER,
  NSExtensionPrincipalClass = "IPlugAUViewController_" + objc_prefix()
                             )
  auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'] = [{}]
  auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['description'] = config['PLUG_NAME']
  auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['manufacturer'] = config['PLUG_MFR_ID']
  auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['name'] = config['PLUG_MFR'] + ": " + config['PLUG_NAME']
  auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['subtype'] = config['PLUG_UNIQUE_ID']
  auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['type'] = COMPONENT_TYPE
  auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['version'] = config['PLUG_VERSION_INT']
  auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['sandboxSafe'] = True
  auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['tags'] = [{}]

  if config['PLUG_TYPE'] == 1:
    auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['tags'][0] = "Synth"
  else:
    auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['tags'][0] = "Effects"

  dump_plist_atomic(plistpath, auv3)
# AUDIOUNIT v3 FRAMEWORK

  plistpath = projectpath + "/resources/" + resource_bundle_name + "-macOS-AUv3Framework-Info.plist"
  auv3framework = load_plist(plistpath)
  # Must match the appex's NSExtensionAttributes/AudioComponentBundle above, or AUv3 instantiation fails.
  auv3framework['CFBundleIdentifier'] = config['BUNDLE_DOMAIN'] + "." + config['BUNDLE_MFR'] + ".app." + config['BUNDLE_NAME'] + ".AUv3Framework"
  auv3framework['CFBundleVersion'] = CFBundleVersion
  auv3framework['CFBundleShortVersionString'] = CFBundleVersion

  dump_plist_atomic(plistpath, auv3framework)
# AAX

  plistpath = projectpath + "/resources/" + resource_bundle_name + "-AAX-Info.plist"
  aax = load_plist(plistpath)
  aax['CFBundleExecutable'] = config['BUNDLE_NAME']
  aax['CFBundleGetInfoString'] = CFBundleGetInfoString
  aax['CFBundleIdentifier'] = config['BUNDLE_DOMAIN'] + "." + config['BUNDLE_MFR'] + ".aax." + config['BUNDLE_NAME'] + ""
  aax['CFBundleName'] = config['BUNDLE_NAME']
  aax['CFBundleVersion'] = CFBundleVersion
  aax['CFBundleShortVersionString'] = CFBundleVersion
  aax['LSMinimumSystemVersion'] = LSMinimumSystemVersion
  aax['CSResourcesFileMapped'] = CSResourcesFileMapped

  dump_plist_atomic(plistpath, aax)
# APP

  plistpath = projectpath + "/resources/" + resource_bundle_name + "-macOS-Info.plist"
  macOSapp = load_plist(plistpath)
  macOSapp['CFBundleExecutable'] = config['BUNDLE_NAME']
  macOSapp['CFBundleGetInfoString'] = CFBundleGetInfoString
  macOSapp['CFBundleIdentifier'] = config['BUNDLE_DOMAIN'] + "." + config['BUNDLE_MFR'] + ".app." + config['BUNDLE_NAME'] + ""
  macOSapp['CFBundleName'] = config['BUNDLE_NAME']
  macOSapp['CFBundleVersion'] = CFBundleVersion
  macOSapp['CFBundleShortVersionString'] = CFBundleVersion
  macOSapp['LSMinimumSystemVersion'] = LSMinimumSystemVersion
  macOSapp['CFBundlePackageType'] = "APPL"
  macOSapp['CFBundleSignature'] = config['PLUG_UNIQUE_ID']
  macOSapp['CSResourcesFileMapped'] = CSResourcesFileMapped
  macOSapp['NSPrincipalClass'] = "SWELLApplication"
  macOSapp['NSMainNibFile'] = resource_bundle_name + "-macOS-MainMenu"
  macOSapp['LSApplicationCategoryType'] = "public.app-category.music"
  macOSapp['CFBundleIconFile'] = resource_bundle_name + ".icns"
#  macOSapp['NSMicrophoneUsageDescription'] = 	"This app needs mic access to process audio."

  dump_plist_atomic(plistpath, macOSapp)
if __name__ == '__main__':
  main()
