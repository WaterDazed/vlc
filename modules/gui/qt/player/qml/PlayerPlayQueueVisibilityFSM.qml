/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
import QtQuick

import VLC.MainInterface
import VLC.Util

/**
 * playqueue visibility state machine
 *
 * @startuml
 * state Floating {
 * }
 * state Docked {
 *    state Visible {
 *    }
 *    state Hidden {
 *       state followVisible {
 *        }
 *       state embed {
 *       }  
 *    }
 * }
 * @enduml
 *
 */
FSM {
    id: fsm
 
    //incoming signals

    //user clicked on the playqueue button
    signal togglePlayQueueVisibility()
    //playqueue visibility update externally
    signal updatePlayQueueVisible()
    signal updatePlayQueueDocked()
    signal updateVideoEmbed()

    //exposed internal states
    property alias isPlayQueueVisible: fsmVisible.active
 
    initialState: MainCtx.playqueueDocked ? fsmDocked : fsmFloating
    
    signalMap: ({
        togglePlayQueueVisibility: fsm.togglePlayQueueVisibility,
        updatePlayQueueVisible: fsm.updatePlayQueueVisible,
        updatePlayQueueDocked: fsm.updatePlayQueueDocked,
        updateVideoEmbed: fsm.updateVideoEmbed,
    })
 
    FSMState {
        id: fsmFloating
 
        transitions: ({
            togglePlayQueueVisibility: {
                action: () => { MainCtx.playqueueVisible = !MainCtx.playqueueVisible }
            },
            updatePlayQueueDocked: {
                guard: () => MainCtx.playqueueDocked,
                target: fsmDocked
            }
        })
    }
 
    FSMState {
        id: fsmDocked
 
        initialState: (MainCtx.hasEmbededVideo || !MainCtx.playqueueVisible )
                      ? fsmHidden : fsmVisible
 
        transitions: ({
            updatePlayQueueDocked: {
                guard: () => !MainCtx.playqueueDocked,
                target: fsmFloating
            },
        })
 
        FSMState {
            id: fsmVisible

            function enter() {
                MainCtx.playqueueVisible = true
            }
 
            transitions: ({
                updateVideoEmbed: {
                    guard: () => MainCtx.hasEmbededVideo,
                    target: fsmHidden
                },
                updatePlayQueueVisible: {
                    guard: () => !MainCtx.playqueueVisible,
                    target: fsmFollowVisible
                },
                togglePlayQueueVisibility: {
                    target: fsmFollowVisible
                },
            })
        }
 
        FSMState {
            id: fsmHidden

            initialState: MainCtx.hasEmbededVideo ? fsmEmbed : fsmFollowVisible

            FSMState {
                id: fsmFollowVisible

                function enter() {
                    MainCtx.playqueueVisible = false
                }

                transitions: ({
                    updateVideoEmbed: {
                        guard: () => MainCtx.hasEmbededVideo,
                        target: fsmEmbed
                    },
                    updatePlayQueueVisible: {
                        guard: () => MainCtx.playqueueVisible,
                        target: fsmVisible
                    },
                    togglePlayQueueVisibility: {
                        target: fsmVisible
                    },
                })
            }

            FSMState {
                id: fsmEmbed

                transitions: ({
                    updateVideoEmbed: [{ //guards tested in order{
                        guard: () => !MainCtx.hasEmbededVideo && !MainCtx.playqueueVisible,
                        target: fsmFollowVisible
                    }, {
                        guard: () => !MainCtx.hasEmbededVideo && MainCtx.playqueueVisible,
                        target: fsmVisible
                    }],
                    togglePlayQueueVisibility: {
                        target: fsmVisible
                    },
                    updatePlayQueueVisible: {
                        guard: () => MainCtx.playqueueVisible,
                        target: fsmVisible
                    },
                })
            }
        }
    }
}
