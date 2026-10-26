/* Main executable of a game .app made by new_game.sh.
 *
 * A bundle whose executable is a shell script runs as /bin/zsh, so macOS privacy checks (TCC)
 * see zsh instead of the app and silently deny access to removable volumes, where the game
 * usually lives. This small binary gives the app its own identity: it starts
 * Contents/Resources/launch.zsh as a child process (not exec, which would turn this process
 * back into zsh) and waits for it, so the access prompt is shown for the game app. */
#include <libgen.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

extern char **environ;

int main(void) {
    char exe[PATH_MAX], real[PATH_MAX], script[PATH_MAX];
    uint32_t size = sizeof exe;
    if (_NSGetExecutablePath(exe, &size) != 0 || !realpath(exe, real)) return 1;
    snprintf(script, sizeof script, "%s/../Resources/launch.zsh", dirname(real));
    char *argv[] = {"/bin/zsh", script, NULL};
    pid_t pid;
    if (posix_spawn(&pid, "/bin/zsh", NULL, NULL, argv, environ) != 0) return 1;
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {}
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
