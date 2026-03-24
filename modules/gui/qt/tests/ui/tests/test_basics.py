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


from pathlib import Path
from time import sleep
from vlc_gui_automation import VLCTests, windows_only, disabled, get_music_dir, get_videos_dir, Keys

class TestBasics(VLCTests):
    video_list = ["anamorph/anamorph.mkv"]
    music_list = ["artwork_from_album/track.mp3"]
    vlc_args = VLCTests.vlc_args + [] # Extra VLC args if needed

    def test_open_video_file(self):
        self.vlc.open_media_by_path(str(Path(get_videos_dir(), "anamorph.mkv")))
        sleep(80)

    def test_open_music_file(self):
        self.vlc.open_media_by_path(str(Path(get_music_dir(), "track.mp3")))
        sleep(15)

    def test_long_presses(self):
        for _ in range(5):
            self.utils.send_special_key(Keys.TAB)
        self.utils.long_keypress(Keys.SPACE, duration=2) 

    def test_navigate_to_all_tabs(self):
        self.vlc.navigate_to_discover_tab()
        self.vlc.navigate_to_browse_tab()
        self.vlc.navigate_to_music_tab()
        self.vlc.navigate_to_video_tab()
        self.vlc.navigate_to_home_tab()

    # Does not work currently because of QT bug: https://bugreports.qt.io/browse/QTBUG-110624
    @disabled
    def test_open_vlc_preferences_legacy(self):
        self.utils.send_special_key(Keys.F10)
        sleep(1)
        self.utils.click_element_by_name("&Playback")
        self.utils.click_element_by_name("Chapter")
    
    # Does not work currently because of QT bug: https://bugreports.qt.io/browse/QTBUG-110624
    @disabled
    def test_open_vlc_preferences_new(self):
        self.utils.click_element_by_name("menuItem")
        self.utils.click_element_by_name("P&layback")

    @disabled # Works only on Qt 6.9+ 
    def test_open_preferences_keyboard_and_navigate_to_ml(self):
        self.vlc.navigate_to_media_library_category()

    def test_take_screenshot(self):
        self.utils.take_screenshot()
