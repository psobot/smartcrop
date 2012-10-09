# SmartCrop
by Peter Sobot (psobot.com) on August 26, 2012. Licensed under MIT.

---

SmartCrop is a smart JPEG thumbnailing algorithm based on entropy measurements of a sliding window.
Based off an [algorithm by Michael Macias](https://gist.github.com/a54cd41137b678935c91), SmartCrop
was first [implemented in Ruby](https://gist.github.com/2440571) back in April 2012. This solution,
however, was dog-slow. (On the order of ~2 seconds per image processed!) This new version is
implemented in raw C++, using libJPEG, and is literally **50 times faster** than the Ruby version.

There certainly exist bugs in this implementation... but that's why this is open source!

---

## Usage:

    SmartCrop by Peter Sobot
    ------------------------
    Usage: ./smartcrop [-o output_dir] [-d date_from] input_file input_file ...
           Default output directory is the current directory.
           Default date from is forever ago.
