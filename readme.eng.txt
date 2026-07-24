1. What's here.
-------------
Kronos3vm is a software emulator of the Kronos computer, which was developed from 1984 to 1991 in Novosibirsk. The emulator runs the Excelsior iV operating system and also includes a copy of all the Excelsior OS source code, ready for system builds.
In the doc subfolder, you'll find documentation for Kronos processors and the Excelsior OS.

More information about Kronos can be found here: https://kronos.ru/
GitHub: https://github.com/leok7v/kronos
GitHub clone: ​​https://github.com/hady-exc/kronos

2. How to start.
-----------------
In the Windows command prompt, type:

  Kronos3vm xd0.dsk xd1.dsk ENTER

The emulator will start, load the OS, and you'll receive a prompt.

  user:

In response, type sys ENTER

After logging in, the prompt will look like this:

  17:12 sys !

where 17:12 is the current time and sys is the name of the current directory.
Next, you can enter commands. More about the commands see below, but try "ls" first.

3. Configuring the Windows Command Prompt window.
-------------------------------------------
This configuration is optional; most utilities will work without it, so you can postpone the configuration. However, the EX text editor will be nearly impossible to use due to the distorted text layout on the screen, so if you need it, follow these steps.

Step 1.
This step is only necessary if you have Windows 11. If you are using older versions of Windows, you can skip it.
On Windows 11, you need to switch the terminal to the "old" Windows Console Host instead of the default Windows Terminal.
- Open Settings -> System -> For developers.
- Find the "Terminal" box in the list, and select "Windows Console Host" from the drop-down list on the right.
- Close Settings

Step 2.
Set the console window and window buffer size to 50 lines by 80 characters.
To do this:
- Launch the command prompt, right-click the left icon in the window title bar, and select either Properties or Defaults from the drop-down menu. The first option will adjust only this window; the second will set the new values ​​for all console windows that are launched after this.
- In the settings window that appears, set both the window and window buffer size to 50 lines by 80 characters.
- If necessary, go to the Font tab and select a font that will allow the entire window to fit on your screen.
- Click the OK button.

What's next?
-----------
The Excelsior OS command interface is very similar to any Unix. The ls and cd utilities help you explore the contents of your disk. There's also a Norton Commander-like tool called kc, which is more convenient. There are also cp, rm, ln, cat, and much more.
File editor - ex.
Modula-2 compiler - mx (note: the documentation calls it m2, but it is outdated at this point).
Review the documentation; it lists the other utilities available for launch.
.bat file equivalents have the extension ".@" or "sh," and executable files have the extension ".cod."
To launch an executable file, simply type its name without the extension.
Most utilities provide a prompt when launched with the "-h" switch.

Good luck!