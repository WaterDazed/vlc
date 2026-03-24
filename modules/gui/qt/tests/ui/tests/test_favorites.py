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

from vlc_gui_automation import VLCTests, disabled, Keys
import platform

class TestFavorites(VLCTests):
    video_list = ["anamorph/anamorph.mkv"]
    music_list = ["artwork_from_album/track.mp3"]
    vlc_args = VLCTests.vlc_args + [] # Extra VLC args if needed

    def test_favorite_music_only_with_keyboard_navigation(self):
        nb_it = 5 if platform.system() == "Windows" else 3
        for _ in range(nb_it):
            self.utils.send_special_key(Keys.TAB)
        self.utils.send_special_key(Keys.SPACE)
        for _ in range(7):
            self.utils.send_special_key(Keys.TAB)
        self.utils.send_special_key(Keys.SPACE)
        for _ in range(6):
            self.utils.send_special_key(Keys.TAB)
        self.utils.send_special_key(Keys.MENU)
        for _ in range(3):
            self.utils.send_special_key(Keys.TAB)
        self.utils.send_special_key(Keys.ENTER)
        for _ in range(2):
            self.utils.send_special_key(Keys.TAB)
        self.utils.send_special_key(Keys.SPACE)
        self.assertTrue(self.vlc.verify_is_item_in_favorites("Track 1"), "Track 1 was not found in favorites.")


    @disabled
    def test_favorite_music_and_verify_if_its_applied(self):
        self.utils.click_element_by_name("musicNavBar")
        self.utils.click_element_by_name("tracksSubNavbar")

        child = self.utils.find_element_by_name("Track 1")
        parent = self.utils.get_element_parent(child)
        self.utils.hover_element(parent)
        self.utils.click_element_by_name(parent, "menuButtonTrack")
        self.utils.click_element_by_name("Add to favorites") # Currently not working because of this Qt bug: https://bugreports.qt.io/browse/QTBUG-110624
        self.utils.click_element_by_name("homeNavBar")
        self.assertTrue(self.vlc.verify_is_item_in_favorites("Track 1"), "Track 1 was not found in favorites.")