import sys
import os
import pygame
from mutagen.easyid3 import EasyID3
import pyrebase
import time
from pynput import keyboard
from PyQt5.QtWidgets import QApplication, QWidget, QVBoxLayout, QGridLayout, QPushButton, QLabel, QLineEdit
from PyQt5.QtCore import Qt, QSize, QTimer
from PyQt5.QtGui import QIcon, QPixmap
from luma.core.interface.serial import i2c
from luma.oled.device import ssd1306
from PIL import Image, ImageDraw, ImageFont

# Inicializar pygame para la reproducción de música
pygame.init()
pygame.mixer.init()

# Ruta de la carpeta que contiene los archivos de música
carpeta_musica = r"/home/itzel/Documentos/Top +100 éxitos de Carlos Ramón"
# Obtener la lista de archivos de música en la carpeta
archivos_musica = [archivo for archivo in os.listdir(carpeta_musica) if archivo.endswith(".mp3")]

# Configuración de Firebase
firebaseConfig = {
    'apiKey': "AIzaSyCfowCIIFNHU_QGtRH0hKxNfshiyvsdSkk",
    'authDomain': "soc-reto.firebaseapp.com",
    'databaseURL': "https://soc-reto-default-rtdb.firebaseio.com",
    'projectId': "soc-reto",
    'storageBucket': "soc-reto.appspot.com",
    'messagingSenderId': "531255716953",
    'appId': "1:531255716953:web:d4b93cc82277012b205548"
}

firebase = pyrebase.initialize_app(firebaseConfig)
db = firebase.database()

# Intentar configurar la pantalla OLED
try:
    serial = i2c(port=1, address=0x3C)
    device = ssd1306(serial)
    oled_found = True
    print("Se ha encontrado la pantalla")
except Exception as e:
    oled_found = False
    print("No se ha encontrado la OLED")

class MusicPlayer(QWidget):
    def __init__(self):
        super().__init__()
        self.initUI()
        self.pausa = False
        self.numero = self.obtener_estado_desde_firebase()
        self.ultimo_estado = self.numero

        if self.numero:
            self.reproducir_por_indice(self.numero)

        # Configurar el evento de finalización de la canción
        pygame.mixer.music.set_endevent(pygame.USEREVENT)
        
        # Iniciar el listener para las teclas presionadas
        self.listener = keyboard.Listener(on_press=self.on_press)
        self.listener.start()

        # Bucle principal
        self.timer = QTimer()
        self.timer.timeout.connect(self.check_firebase_update)
        self.timer.start(1000)  # Verificar cada segundo

    def initUI(self):
        self.setWindowTitle('Reproductor de Música Mbot MEGA')
        self.setGeometry(100, 100, 1000, 300)

        layout = QVBoxLayout()
        grid_layout = QGridLayout()

        self.artista_label = QLabel('Artista: ', self)
        self.artista_label.setStyleSheet("background-color: black; color: white;")
        grid_layout.addWidget(self.artista_label, 0, 0, 1, 3)

        self.numero_label = QLabel(self)
        self.numero_label.setStyleSheet("background-color: black; color: white;")
        grid_layout.addWidget(self.numero_label, 1, 0, 1, 3)

        self.btn_atras = self.create_button_with_image(r'/home/itzel/Imágenes/Nueva carpeta/atras.png')
        self.btn_atras.clicked.connect(self.retroceder_cancion)
        grid_layout.addWidget(self.btn_atras, 2, 0)

        self.btn_pausa = self.create_button_with_image(r'/home/itzel/Imágenes/Nueva carpeta/pausa.png')
        self.btn_pausa.clicked.connect(self.pausar_reproducir)
        grid_layout.addWidget(self.btn_pausa, 2, 1)

        self.btn_siguiente = self.create_button_with_image(r'/home/itzel/Imágenes/Nueva carpeta/detener.png')
        self.btn_siguiente.clicked.connect(self.avanzar_cancion)
        grid_layout.addWidget(self.btn_siguiente, 2, 2)

        self.input_numero = QLineEdit(self)
        self.input_numero.setPlaceholderText('Ingrese el número de la canción')
        self.input_numero.setStyleSheet("background-color: black; color: white;")
        self.input_numero.returnPressed.connect(self.buscar_cancion_por_numero)  # Conectar evento returnPressed
        grid_layout.addWidget(self.input_numero, 4, 0, 1, 2)

        self.btn_repetir = QPushButton('Repetir canción', self)
        self.btn_repetir.clicked.connect(self.repetir_cancion)
        grid_layout.addWidget(self.btn_repetir, 4, 2, 1, 1)
        self.btn_repetir.setStyleSheet("color: #f3f4f7;")

        self.btn_w = self.create_button_with_image(r'/home/itzel/Imágenes/Nueva carpeta/w.png')
        self.btn_w.clicked.connect(lambda: self.enviar_motor('A'))
        grid_layout.addWidget(self.btn_w, 0, 7)

        self.btn_a = self.create_button_with_image(r'/home/itzel/Imágenes/Nueva carpeta/a.png')
        self.btn_a.clicked.connect(lambda: self.enviar_motor('B'))
        grid_layout.addWidget(self.btn_a, 1, 6)

        self.btn_d = self.create_button_with_image(r'/home/itzel/Imágenes/Nueva carpeta/d.png')
        self.btn_d.clicked.connect(lambda: self.enviar_motor('D'))
        grid_layout.addWidget(self.btn_d, 1, 8)

        self.btn_s = self.create_button_with_image(r'/home/itzel/Imágenes/Nueva carpeta/s.png')
        self.btn_s.clicked.connect(lambda: self.enviar_motor('C'))
        grid_layout.addWidget(self.btn_s, 2, 7)

        self.btn_panzon = self.create_button_with_image(r'/home/itzel/Imágenes/Nueva carpeta/detener.png')
        self.btn_panzon.clicked.connect(lambda: self.enviar_motor('0'))
        grid_layout.addWidget(self.btn_panzon, 1, 7)
        
        layout.addLayout(grid_layout)
        self.setLayout(layout)
        self.setStyleSheet("background-color: #52565e;")

    def create_button_with_image(self, image_path):
        button = QPushButton()
        pixmap = QPixmap(image_path)
        icon = QIcon(pixmap)
        button.setIcon(icon)
        button.setIconSize(QSize(50, 50))
        return button

    def obtener_estado_desde_firebase(self):
        cancion = db.child("test").child("cancion").get().val()
        return int(cancion) if cancion and cancion.isdigit() else None

    def reproducir_musica(self, archivo):
        pygame.mixer.music.load(archivo)
        pygame.mixer.music.play()

    def obtener_info_cancion(self, archivo):
        try:
            audio = EasyID3(os.path.join(carpeta_musica, archivo))
            artista = audio['artist'][0]
            titulo = audio['title'][0]
        except Exception as e:
            print(f"No se pudo obtener la información para {archivo}: {e}")
            artista = "Desconocido"
            titulo = os.path.splitext(archivo)[0]
        return artista, titulo
    
    def reproducir_por_indice(self, indice):
        if 1 <= indice <= 103:
            numero_formateado = f"{indice:02d}"
            archivo = next((archivo for archivo in archivos_musica if archivo.startswith(numero_formateado)), None)
            if archivo:
                artista, titulo = self.obtener_info_cancion(archivo)
                self.numero_label.setText(f' {indice} - {titulo}')
                self.artista_label.setText(f' Artista: {artista}')
                print(f"Reproduciendo: {artista} - {titulo}")
                self.reproducir_musica(os.path.join(carpeta_musica, archivo))
                if oled_found:
                    self.mostrar_en_pantalla_oled(artista, titulo)
            else:
                print("No se encontró la canción correspondiente")
        else:
            print("Número de canción inválido. Intente nuevamente.")
            
    def mostrar_en_pantalla_oled(self, artista, titulo):
        if oled_found:
            # Crear una imagen en blanco
            image = Image.new("1", (device.width, device.height), "black")
            draw = ImageDraw.Draw(image)
            
            # Configurar la fuente
            font = ImageFont.load_default()
            
            # Escribir el texto en la imagen
            draw.text((10, 10), f"Artista: {artista}", fill="white", font=font)
            draw.text((10, 30), f"{titulo}", fill="white", font=font)
            
            # Mostrar la imagen en la pantalla OLED
            device.display(image)
        
    def retroceder_cancion(self):
        self.numero -= 1
        if self.numero < 1:
            self.numero = len(archivos_musica)
        db.child("test").update({"cancion": str(self.numero)})
        self.reproducir_por_indice(self.numero)

    def pausar_reproducir(self):
        if self.pausa:
            pygame.mixer.music.unpause()  # Reanudar la música si está en pausa
            self.pausa = False
        else:
            pygame.mixer.music.pause()  # Pausar la música si se está reproduciendo
            self.pausa = True

    def avanzar_cancion(self):
        self.numero += 1
        if self.numero > len(archivos_musica):
            self.numero = 1
        db.child("test").update({"cancion": str(self.numero)})
        self.reproducir_por_indice(self.numero)

    def repetir_cancion(self):
        db.child("test").update({"cancion": str(self.numero)})
        self.reproducir_por_indice(self.numero)
        self.pausa = False
            
    def buscar_cancion_por_numero(self):
        numero = self.input_numero.text()
        if numero.isdigit():
            numero = int(numero)
            if 1 <= numero <= len(archivos_musica):
                db.child("test").update({"cancion": str(numero)})
                self.reproducir_por_indice(numero)
                self.pausa = False
            else:
                print("Número de canción inválido.")
        else:
            print("Por favor, ingrese un número válido.")

    def enviar_motor(self, motor):
        db.child("test").update({"motor": motor})
        print(f"Función {motor} enviada a Firebase")

    def on_press(self, key):
        try:
            if key.char == 'q':
                pygame.mixer.music.stop()
                return False
            elif key.char == 'j':
                self.retroceder_cancion()
            elif key.char == 'k':
                self.pausar_reproducir()
            elif key.char == 'l':
                self.avanzar_cancion()
            elif key.char is not None:
                nuevo_numero = int(key.char)
                if 1 <= nuevo_numero <= len(archivos_musica):
                    db.child("test").update({"cancion": str(nuevo_numero)})
                    self.reproducir_por_indice(nuevo_numero)
                    self.pausa = False
                else:
                    print("Número de canción inválido.")
        except AttributeError:
            pass

    def check_firebase_update(self):
        nuevo_estado = self.obtener_estado_desde_firebase()
        if nuevo_estado and nuevo_estado != self.ultimo_estado:
            self.ultimo_estado = nuevo_estado
            self.numero = nuevo_estado
            self.reproducir_por_indice(self.numero)
            self.pausa = False
        
        # Revisar la variable 'reproductor' en Firebase
        reproductor_val = db.child("test").child("reproductor").get().val()
        if reproductor_val:
            self.handle_reproductor_input(reproductor_val)

    def handle_reproductor_input(self, value):
        if value == 'A':
            self.retroceder_cancion()
        elif value == 'B':
            self.pausar_reproducir()
        elif value == 'C':
            self.avanzar_cancion()
        elif value == 'D':
            self.repetir_cancion()
        elif value == 'F':
            pass  # No hacer nada si el valor es F
        # Mantener el valor de reproductor como F después de cualquier acción
        db.child("test").update({"reproductor": "F"})

    def handle_song_end(self, event):
        if event.type == pygame.USEREVENT:
            self.avanzar_cancion()

if __name__ == '__main__':
    app = QApplication(sys.argv)
    ex = MusicPlayer()

    # Establecer el ícono de la ventana
    icon_path = r'/home/itzel/Imágenes/Nueva carpeta/00_mbot Mega.png'
    app.setWindowIcon(QIcon(icon_path))

    ex.show()

    # Conectar el evento de finalización de la canción
    pygame.mixer.music.set_endevent(pygame.USEREVENT)
    pygame.event.set_allowed(pygame.USEREVENT)

    # Crear un temporizador para verificar eventos de Pygame
    def check_pygame_events():
        for event in pygame.event.get():
            if event.type == pygame.USEREVENT:
                ex.handle_song_end(event)

    # Conectar el temporizador para verificar eventos de Pygame
    timer = QTimer()
    timer.timeout.connect(check_pygame_events)
    timer.start(100)  # Verificar cada 100 ms

    sys.exit(app.exec_())
