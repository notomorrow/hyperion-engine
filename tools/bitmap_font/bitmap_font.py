from PIL import Image, ImageFont, ImageDraw
from math import sqrt

class ImageDrawer:
    def __init__(self):
        self.num_chars_per_row = 16
        self.num_chars_per_col = 16
        self.image_size = (2048, 2048)
        self.canvas = Image.new('RGBA', self.image_size)
        self.draw = ImageDraw.Draw(self.canvas)
        self.font = ImageFont.truetype('Monaco', int(self.char_width), encoding='unic')

    @property
    def char_width(self):
        return self.image_size[0] / self.num_chars_per_row

    @property
    def char_height(self):
        return self.image_size[1] / self.num_chars_per_col

    def draw_charset(self):
        pos = [0, 0]

        for i in range(32, 127):
            self.draw_char(chr(i), pos)
            self.next_char_pos(pos)

    def draw_char(self, ch, pos):
        (left, top, right, bottom) = self.font.getbbox(ch)
        text_width, text_height = right - left, bottom - top
        half_char_width = self.char_width / 2
        half_char_height = self.char_height / 2
        half_text_width = text_width / 2
        half_text_height = text_height / 2

        offset_pos = (pos[0] + (half_char_width - half_text_width), pos[1])

        self.draw.text(offset_pos, ch, fill=None, font=self.font)
    
    def next_char_pos(self, pos):
        pos[0] += self.char_width

        if pos[0] >= self.image_size[0]:
            pos[0] = 0
            pos[1] += self.char_height
        

def main():
    drawer = ImageDrawer()
    drawer.draw_charset()

    drawer.canvas.save('bitmap_font.png', 'PNG')
    drawer.canvas.show()

if __name__ == '__main__':
    main()
