#! /usr/bin/python

from PyQt4.QtCore import Qt
from PyQt4.QtGui import QApplication, QColor
from PyQt4.QtOpenGL import QGLWidget

from OpenGL import GL, GLU
# from OpenGL.GLU import *
import sys, os
import PIL.Image
import numpy

Opacity = 0.75

def loadTexture(name):

	img = PIL.Image.open(name) # .jpg, .bmp, etc. also work
	img_data = numpy.array(list(img.getdata()), numpy.int8)

	id = GL.glGenTextures(1)
	GL.glPixelStorei(GL.GL_UNPACK_ALIGNMENT,1)
	GL.glBindTexture(GL.GL_TEXTURE_2D, id)
	GL.glTexParameterf(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_WRAP_S, GL.GL_CLAMP)
	GL.glTexParameterf(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_WRAP_T, GL.GL_CLAMP)
	GL.glTexParameterf(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_MAG_FILTER, GL.GL_LINEAR)
	GL.glTexParameterf(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_MIN_FILTER, GL.GL_LINEAR)
	GL.glTexImage2D(GL.GL_TEXTURE_2D, 0, GL.GL_RGB, img.size[0], img.size[1], 0, GL.GL_RGB, GL.GL_UNSIGNED_BYTE, img_data)
	return id

def drawImg(x,y,w,h,tex,r=1,g=1,b=1,a=1):
	GL.glColor4f(r,g,b,a);
	t = [0, 0, 1, 0, 1, 1, 0, 1 ]
	v = [x, y, x+w, y, x+w, y+h, x, y+h];

	GL.glVertexPointer( 2, GL.GL_FLOAT, 0, v );
	GL.glEnableClientState( GL.GL_VERTEX_ARRAY );

	GL.glTexCoordPointer( 2, GL.GL_FLOAT, 0, t );
	GL.glEnableClientState( GL.GL_TEXTURE_COORD_ARRAY );
	
	GL.glEnable(GL.GL_TEXTURE_2D);
	GL.glBindTexture(GL.GL_TEXTURE_2D, tex);
	GL.glDrawArrays( GL.GL_TRIANGLE_FAN, 0, 4 );

tex = loadTexture("ring.png")

class File(object):
	"""docstring for File"""
	def __str__(self):
		return "<File "+self.path+">"
	def __repr__(self):
		return str(self)
	def __init__(self, path):
		super(File, self).__init__()
		self.path = path

class Folder(File):
	"""docstring for Folder"""
	def __str__(self):
		return "<Folder "+self.path+">"
	def __init__(self, path):
		super(Folder, self).__init__(path)
	def read(self):
		# List of items in the directory. If it's not a file or a folder, add it as None, and then filter out all Nones from the list.
		self.files = filter(None,[File(i) if os.path.isfile(i) else Folder(i) if os.path.isdir(i) else None for i in [self.path+i for i in os.listdir(self.path)]])
	def draw(self):
		'''
		GL.glEnable(GL.GL_TEXTURE_2D)
		GL.glBindTexture(GL.GL_TEXTURE_2D, tex)
		# GL.glBegin(GL.GL_QUADS);
		GL.glBegin(GL.GL_TRIANGLES);
		GL.glTexCoord2f(0.0, 0.0);   GL.glVertex3f(-0.9, -0.9, 0.0)
		GL.glTexCoord2f(1.0, 0.0);   GL.glVertex3f( 0.9, -0.9, 0.0)
		GL.glTexCoord2f(1.0, 1.0);   GL.glVertex3f( 0.9, 0.9, 0.0)
		GL.glEnd()'''
		drawImg(128,128,256,256,tex,0.5,0.75,1.0,1.0);


class TransGLWidget(QGLWidget):
	def __init__(self, parent=None):
		QGLWidget.__init__(self, parent)
		self.setWindowFlags(Qt.FramelessWindowHint)
		self.setAttribute(Qt.WA_TranslucentBackground, True)
		# self.setGeometry(200, 100, 640, 480)
		self.showFullScreen()
		self.root = Folder('/')
		self.root.read()
		print self.root.files

	def initializeGL(self):
		self.qglClearColor(QColor(0, 0, 0, 0))

	def resizeGL(self, w, h):
		# GL.glViewport(0, 0, w, h)
		# GL.glMatrixMode(GL.GL_PROJECTION)
		# GL.glLoadIdentity()
		# x = float(w) / h
		# GL.glFrustum(-x, x, -1.0, 1.0, 1.0, 10.0)
		# GL.glMatrixMode(GL.GL_MODELVIEW)
		GL.glLoadIdentity()
		GLU.gluOrtho2D(0, w, 0, h);

	def paintGL(self):
		GL.glClearColor(0.0, 0.0, 0.0, Opacity)
		GL.glClear(GL.GL_COLOR_BUFFER_BIT | GL.GL_DEPTH_BUFFER_BIT)   

		'''GL.glEnable(GL.GL_TEXTURE_2D)
		GL.glBindTexture(GL.GL_TEXTURE_2D, tex)

		GL.glBegin(GL.GL_TRIANGLES)
		# GL.glColor3f(1.0, 0.0, 0.0)
		GL.glTexCoord2f(0.0, 0.0);
		GL.glVertex3f(-1.0, 1.0, -3.0)
		# GL.glColor3f(0.0, 1.0, 0.0)
		GL.glTexCoord2f(1.0, 0.0);
		GL.glVertex3f(1.0, 1.0, -3.0)
		# GL.glColor3f(0.0, 0.0, 1.0)
		GL.glTexCoord2f(1.0, 1.0);
		GL.glVertex3f(0.0, -1.0, -3.0)
		GL.glEnd()'''
		self.root.draw()

app = QApplication(sys.argv)
widget = TransGLWidget()    
widget.show()
app.exec_()