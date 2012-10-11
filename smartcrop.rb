# Hacky random image thumbnailer.
# by Peter Sobot, April 21, 2012
# Based heavily on code by Michael Macias
#   (https://gist.github.com/a54cd41137b678935c91)

require 'rmagick'
require 'date'

images = Dir.glob ARGV[0]
output_dir = ARGV[1]
if ARGV[2]
  since = Date.parse(ARGV[2]).to_time
  images = images.find_all { |f| File.mtime(f) >= since }
end

num = Dir.glob(output_dir + '*.jpg').count
lim = 50

def entropy(image)
  hist = image.quantize(256, Magick::GRAYColorspace).color_histogram
  area = (image.rows * image.columns).to_f

  -hist.values.reduce(0.0) do |e, freq|
    p = freq / area
    e + p * Math.log2(p)
  end
end

def smart_crop(image, crop_width, crop_height)
  x, y, width, height = 0, 0, image.columns, image.rows
  slice_length = 16

  while (width - x) > crop_width
    slice_width = [width - x - crop_width, slice_length].min

    left = image.crop(x, 0, slice_width, image.rows)
    right = image.crop(width - slice_width, 0, slice_width, image.rows)

    if entropy(left) < entropy(right)
      x += slice_width
    else
      width -= slice_width
    end
  end

  while (height - y) > crop_height
    slice_height = [height - y - crop_height, slice_length].min

    top = image.crop(0, y, image.columns, slice_height)
    bottom = image.crop(0, height - slice_height, image.columns, slice_height)

    if entropy(top) < entropy(bottom)
      y += slice_height
    else
      height -= slice_height
    end
  end

  im = image.crop(x, y, crop_width, crop_height)
  if im.columns < crop_width or im.rows < crop_height
    raise "Cropped image too small!"
  end
  im
end

puts "Matched #{images.length} images."

images.shuffle.slice(0, lim).each do |file|
  output = output_dir + (num + 1).to_s + '.jpg'
  begin
    a = Time.now
    im = Magick::Image.read(file).first.auto_orient.resize(1.0/8.0)
    r = smart_crop(im, 124, 124)
    r.strip!
    r.write(output) { self.quality = 85 }
    
    num = num + 1
    ms = 1000*(Time.now - a)
    puts "#{file} => #{output} in #{ms}ms"
  rescue StandardError => e
    puts "ERROR: #{file} didn't work: #{e}"
  end
end
