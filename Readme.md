# SmartCrop
by Peter Sobot (psobot.com) on August 26, 2012. Licensed under MIT.

---

SmartCrop is a smart JPEG thumbnailing algorithm based on entropy measurements of a sliding window.
Based off an [algorithm by Michael Macias](https://gist.github.com/a54cd41137b678935c91), SmartCrop
was first [implemented in Ruby](https://gist.github.com/2440571) back in April 2012. This solution,
however, was dog-slow. (On the order of ~2 seconds per image processed!)

This repo contains three versions of the same basic program:
 - `smartcrop.rb`, the original Ruby script using RMagick
 - `fastcrop`, using libJPEG, literally 50 times faster than the Ruby version.
 - `smartcrop`, using Magick++, which is roughly 6x faster than the Ruby.

Check out [the blog post](http://petersobot.com/blog/rewriting-in-cpp-for-fun-speed-and-masochism/)
on comparing the three methods.

---

## General Usage

Just `make`, then run `./smartcrop` or `./fastcrop` (or `ruby smartcrop.rb`):

    SmartCrop by Peter Sobot
    ------------------------
    Usage: ./smartcrop [-o output_dir] [-d date_from] input_file input_file ...
           Default output directory is the current directory.
           Default date from is forever ago.
