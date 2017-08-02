import Image,struct
img = Image.open('blu.tiff')

class CharData(object):
    def __init__(self, data):
        if data is None:
            self.data = [ [] for y in range(7) ]
        else:
            self.data = data

    def concat( self, other ):
        for y in range(7):
            self.data[y] += other.data[y]

    def get_bindata(self):
        ll = len(self.data[0])
        ret=''
        for y in reversed(range(7)):
            for x in self.data[y]:
                ret += struct.pack('BBBB',*x)
        return ret
            

    def save_as(self, name):
        height = 7
        width = len(self.data[0])
        outimg = Image.frombuffer( "RGBA", (width, height),
            self.get_bindata())
        outimg.save(name)
            

def char_at(char_x,char_y):
    """
    char_x specifies, zero based, the x position of the character
    """
    start_x = 1+char_x+ 14*char_x+2
    start_y = 1+char_y*22+1
    print "Starting at %i,%i" %( start_x,start_y)
    return CharData([[ img.getpixel( (start_x+2*x,start_y+2*y) ) for x in range(5)] for y in range(7)])


chardata = CharData(None)
for x in range(16): #0x00 - 0x0F
    chardata.concat( char_at(0,x) )
for x in range(16): #0x10 - 0x1F
    chardata.concat( char_at(0,x) )
for x in range(16): #0x20 - 0x2F
    chardata.concat( char_at(1,x) )
for x in range(16): #0x30 - 0x3F
    chardata.concat( char_at(2,x) )
for x in range(16): #0x40 - 0x4F
    chardata.concat( char_at(3,x) )
for x in range(16): #0x50 - 0x5F
    chardata.concat( char_at(4,x) )
for x in range(16): #0x60 - 0x6F
    chardata.concat( char_at(5,x) )
for x in range(16): #0x70 - 0x7F
    chardata.concat( char_at(6,x) )
for x in range(16): #0x80 - 0x8F
    chardata.concat( char_at(0,x) )
for x in range(16): #0x90 - 0x9F
    chardata.concat( char_at(0,x) )
for x in range(16): #0xA0 - 0xAF
    chardata.concat( char_at(7,x) )
for x in range(16): #0xB0 - 0xBF
    chardata.concat( char_at(8,x) )
for x in range(16): #0xC0 - 0xCF
    chardata.concat( char_at(9,x) )
for x in range(16): #0xD0 - 0xDF
    chardata.concat( char_at(10,x) )
for x in range(16): #0xE0 - 0xEF
    chardata.concat( char_at(11,x) )
for x in range(16): #0xF0 - 0xFF
    chardata.concat( char_at(12,x) )
print chardata.save_as('7x5font.tiff')
