"""Mixer that controls volume using Shared Memory communication with a cpp service."""

from __future__ import unicode_literals
from mopidy import mixer

import os
import pykka
import logging
import posix_ipc
import mmap
import time
import threading

logger = logging.getLogger(__name__)

class GPIOVolumeMixer(pykka.ThreadingActor, mixer.Mixer):

    name = 'gpiovolume'

    def __init__(self, config):
        super(GPIOVolumeMixer, self).__init__(config)
        oldmask = os.umask(0)
        memory = posix_ipc.SharedMemory("/gpiovolume", posix_ipc.O_RDWR | posix_ipc.O_CREAT, 0666, 8)
        os.umask(oldmask)
        self.mapfile = mmap.mmap(memory.fd, memory.size)
        memory.close_fd()

    def __exit__():
        self.mapfile.close()

    def get_volume(self):
        self.mapfile.seek(0)
        return ord(self.mapfile.read_byte())

    def set_volume(self, volume):
        self.mapfile.seek(0)
        self.mapfile.write_byte(chr(volume))
        self.trigger_volume_changed(volume)
        return True

    def get_mute(self):
        self.mapfile.seek(1)
        return ord(self.mapfile.read_byte())==0

    def set_mute(self, mute):
        self.mapfile.seek(1)
        if (mute):
            self.mapfile.write_byte(chr(0))
        else:
            self.mapfile.write_byte(chr(1))

    def on_start(self):
        self._observer = GPIOVolumeObserver(
            mapfile=self.mapfile,
            vol_callback=self.actor_ref.proxy().update_volume,
            amp_on_callback=self.actor_ref.proxy().update_amp_on)
        self._observer.start()
        logger.info('GPIO volume started')

    def on_stop(self):
        self._observer.stop()
        logger.info('GPIO volume stopped')

    def update_volume(self):
        self.trigger_volume_changed(self.get_volume())

    def update_amp_on(self):
        self.trigger_mute_changed(self.get_mute())

class GPIOVolumeObserver(threading.Thread):
    daemon = True
    name = 'gpiovolumeobserver'

    def __init__(self, mapfile, vol_callback, amp_on_callback):
        super(GPIOVolumeObserver, self).__init__()
        self.running = True
        self.mapfile = mapfile
        self.vol_callback = vol_callback
        self.amp_on_callback = amp_on_callback

    def stop(self):
        self.running = False

    def run(self):
        old_vol = 0;
        old_amp_on = 0;
        while self.running:
            time.sleep(0.1)
            self.mapfile.seek(0)
            new_vol = ord(self.mapfile.read_byte())
            new_amp_on = ord(self.mapfile.read_byte())
            if new_vol != old_vol:
                old_vol = new_vol
                self.vol_callback()
            if new_amp_on != old_amp_on:
                old_amp_on = new_amp_on
                self.amp_on_callback()
