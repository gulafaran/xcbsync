#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <libdrm/drm.h>

#include "structs.h"
#include "util.h"
#include "vsync.h"

static bool vsync_init_drm(void) {
    if((globalconf.vsync_drm_fd = open("/dev/dri/card0", O_RDWR)) < 0) {
        unagi_warn("Failed to open DRM device: %s, disabling VSync with DRM", strerror(errno));

        return false;
    }

    return true;
}

static int vsync_wait_drm(void) {
    if(globalconf.vsync_drm_fd < 0)
        return 0;

    int ret = -1;
    drm_wait_vblank_t vbl;
    vbl.request.type = _DRM_VBLANK_RELATIVE;
    vbl.request.sequence = 1;

    do {
        ret = ioctl(globalconf.vsync_drm_fd, DRM_IOCTL_WAIT_VBLANK, &vbl);
        vbl.request.type &= ~_DRM_VBLANK_RELATIVE;
    } while(ret && errno == EINTR);

    if(ret)
        unagi_warn("VBlank ioctl failed, not implemented in this driver?");

    return ret;
}

static bool vsync_init_gl(void) {
    return false;
}

static int vsync_wait_gl(void) {
    /*unsigned vblank_count = 0;

    glXGetVideoSyncSGI(&vblank_count);
    glXWaitVideoSyncSGI(2, (vblank_count + 1) % 2, &vblank_count);*/
    return 0;
}

static bool vsync_init_vulkan(void) {
  return false;
}

static int vsync_wait_vulkan(void) {
    return 0;
}

bool vsync_init(void) {
    bool ret;

    ret = false;

    if(globalconf.vsync){
        if(globalconf.vsync_drm) {
            ret = vsync_init_drm();
        }
        else if(globalconf.vsync_gl) {
            ret = vsync_init_gl();
        }
        else if(globalconf.vsync_vulkan) {
            ret = vsync_init_vulkan();
        }
        else {
            ret = vsync_init_drm();
        }
    }

    return ret;
}


int vsync_wait(void)
{
    int ret;

    ret = 0;

    if(globalconf.vsync){
        if(globalconf.vsync_drm) {
            ret = vsync_wait_drm();
        }
        else if(globalconf.vsync_gl) {
            ret = vsync_wait_gl();
        }
        else if(globalconf.vsync_vulkan) {
            ret = vsync_wait_vulkan();
        }
        else {
            ret = vsync_wait_drm();
        }
    }

    return ret;
}

void vsync_cleanup(void)
{
  if(globalconf.vsync_drm_fd >= 0)
    close(globalconf.vsync_drm_fd);
}
