#!/usr/bin/env python3
# Copyright © 2026 VideoLabs, VLC authors and VideoLAN
#
# Authors: Wassim Lalaoui <wassim@videolabs.io>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.

import unittest, argparse, logging, platform
from pathlib import Path
from .vlc_utils import get_vlc_actions, apply_vlc_data, launch_vlc
if platform.system() == "Windows":
    from .windows.utils import get_driver_utils, cleanup, init_driver, setup_sandbox, revert_sandbox
else:
    from .linux.utils import get_driver_utils, cleanup, init_driver, setup_sandbox, revert_sandbox

VLC_PATH: str = ""

class VLCTests(unittest.TestCase):
    medialib_paths: list[str] | None = None
    video_list: list[str] = []
    music_list: list[str] = []
    vlc_args: list[str] = ["-A", "adummy", "--no-qt-privacy-ask"]
    
    def setUp(self):
        PREFIX_VIDEOS = Path("media_samples/video/")
        PREFIX_MUSIC = Path("media_samples/music/")

        try:
            self.tmp_dir = setup_sandbox()

            apply_vlc_data(
                video_list=[Path(PREFIX_VIDEOS, video) for video in self.video_list],
                music_list=[Path(PREFIX_MUSIC, music) for music in self.music_list],
                vlc_path=VLC_PATH,
                medialib_paths=self.medialib_paths
            )
            self.vlc_process = launch_vlc(VLC_PATH, self.vlc_args)
            self.driver = init_driver(self.vlc_process.pid)
        except Exception:
            self.tearDown()
            raise
        self.utils = get_driver_utils(self.driver)
        self.vlc = get_vlc_actions(self.utils, self)

    def tearDown(self):
        cleanup(self.vlc_process, self.driver)
        revert_sandbox(self.tmp_dir)


parser = argparse.ArgumentParser(description="Automated VLC tests")
parser.add_argument(
    "-p", "--path",
    type = str,
    required = True,
    help = "Path to VLC executable (e.g. C:/Program Files/VideoLAN/VLC/vlc.exe)"
)

args, remaining_args = parser.parse_known_args()

VLC_PATH = args.path

def run_tests():
    if platform.system() != "Windows" and platform.system() != "Linux":
        raise Exception("This testing framework only supports Windows and Linux platforms.")

    logging.basicConfig(level=logging.DEBUG, filename="testing.log")

    TESTS_DIR = str(Path("tests"))

    loader = unittest.TestLoader()
    if remaining_args:
        suite = loader.loadTestsFromName(remaining_args[0])
    else:
        suite = loader.discover(TESTS_DIR)
    
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if not result.wasSuccessful():
        exit(1)

    