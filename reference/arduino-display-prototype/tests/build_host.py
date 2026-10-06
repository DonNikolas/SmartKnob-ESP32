from pathlib import Path
import concurrent.futures, subprocess, sys
base = Path(__file__).resolve().parent
project = base.parent
lib = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else base / 'lvgl-8.3.11'
assert (lib / 'lvgl.h').is_file(), 'Pass the extracted LVGL 8.3.11 source directory as argument.'
out = base / 'objects'
out.mkdir(exist_ok=True)
flags = ['-O1', '-g', '-DLV_CONF_INCLUDE_SIMPLE', '-I'+str(project), '-I'+str(lib), '-I'+str(base)]
def compile_c(src):
    obj = out / (str(src.relative_to(lib)).replace('/', '_') + '.o')
    subprocess.run(['gcc', *flags, '-c', str(src), '-o', str(obj)], check=True, capture_output=True)
    return str(obj)
try:
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        objects = list(pool.map(compile_c, sorted((lib / 'src').rglob('*.c'))))
    subprocess.run(['ar', 'rcs', str(out / 'liblvgl.a'), *objects], check=True)
    subprocess.run(['gcc', *flags, '-c', str(project/'knob_fonts.c'), '-o', str(out/'knob_fonts.o')], check=True)
    sources = [str(base/'check_ui.cpp'), str(base/'test_platform.cpp')]
    sources += [str(project / f) for f in ['input.cpp', 'settings.cpp', 'network_time.cpp', 'backlight.cpp']]
    for name, extra in [('check_ui', []), ('check_ui_wifi', ['-DKNOB_WIFI_SSID="HostTest"'])]:
        subprocess.run(['g++', *flags, *extra, '-std=c++11', '-Wall', '-Wextra', '-Werror', *sources,
                        str(out/'knob_fonts.o'), str(out/'liblvgl.a'), '-lm', '-o', str(base/name)], check=True)
    print('Built against actual LVGL 8.3.11 sources')
except subprocess.CalledProcessError as e:
    print(e.stderr.decode() if isinstance(e.stderr, bytes) else e.stderr)
    raise
