import face_recognition
import os
import time
import mysql.connector
from datetime import datetime
import shutil

# Fungsi untuk memuat semua gambar dari sebuah folder dan mengembalikan encoding wajah beserta nama file-nya
def load_face_encodings_from_folder(folder_path):
    face_encodings = []
    face_names = []
    for filename in os.listdir(folder_path):
        if filename.endswith(".jpg") or filename.endswith(".png"):
            image_path = os.path.join(folder_path, filename)
            image = face_recognition.load_image_file(image_path)
            encodings = face_recognition.face_encodings(image)
            if encodings:
                face_encodings.append(encodings[0])
                face_names.append(filename)
    return face_encodings, face_names

# Muat encoding wajah yang dikenal dari folder tertentu
known_faces_folder = "C:/xampp/htdocs/janimaniabsensi/karyawan/"
known_face_encodings, known_face_names = load_face_encodings_from_folder(known_faces_folder)

# Fungsi untuk memuat dan mengenali wajah dari folder tertentu
def recognize_faces_from_folder(folder_path, gagal_folder, scan_folder):
    recognized_names = []
    for filename in os.listdir(folder_path):
        if filename.endswith(".jpg") or filename.endswith(".png"):
            image_path = os.path.join(folder_path, filename)
            image_to_recognize = face_recognition.load_image_file(image_path)
            face_locations = face_recognition.face_locations(image_to_recognize)
            face_encodings = face_recognition.face_encodings(image_to_recognize, face_locations)

            if face_encodings:
                for face_encoding in face_encodings:
                    matches = face_recognition.compare_faces(known_face_encodings, face_encoding)
                    name = "Unknown"
                    if True in matches:
                        first_match_index = matches.index(True)
                        name = known_face_names[first_match_index]
                        # Move the file to the scan folder and rename it
                        new_path = os.path.join(scan_folder, filename)
                        shutil.move(image_path, new_path)
                        print(f"Moved recognized face image {filename} to {new_path}")
                    else:
                        # Move the file to the gagal folder and rename it
                        new_filename = f"{os.path.splitext(filename)[0]}_{datetime.now().strftime('%Y%m%d_%H%M%S')}{os.path.splitext(filename)[1]}"
                        new_path = os.path.join(gagal_folder, new_filename)
                        shutil.move(image_path, new_path)
                        print(f"Moved unknown face image {filename} to {new_path}")
                    recognized_names.append((name, filename))  # Store both name and original filename
            else:
                # If no face is detected, move the file to the gagal folder
                new_filename = f"{os.path.splitext(filename)[0]}_{datetime.now().strftime('%Y%m%d_%H%M%S')}{os.path.splitext(filename)[1]}"
                new_path = os.path.join(gagal_folder, new_filename)
                shutil.move(image_path, new_path)
                print(f"Moved image with no face detected {filename} to {new_path}")
                recognized_names.append((None, filename))  # Tambahkan None jika tidak ada wajah yang terdeteksi

    return recognized_names

# Fungsi untuk memeriksa mode dari database
def get_mode_from_database():
    try:
        connection = mysql.connector.connect(
            host='localhost',  # Sesuaikan dengan host MySQL Anda
            user='root',  # Sesuaikan dengan username MySQL Anda
            password='',  # Sesuaikan dengan password MySQL Anda
            database='janimani'  # Sesuaikan dengan nama database MySQL Anda
        )
        cursor = connection.cursor()
        cursor.execute("SELECT `mode` FROM `kondisi` WHERE id = 1")
        mode = cursor.fetchone()[0]
        return mode
    except mysql.connector.Error as error:
        print("Failed to get mode from database: {}".format(error))
        return None
    finally:
        if connection.is_connected():
            cursor.close()
            connection.close()
            print("MySQL connection is closed")

# Fungsi untuk mengupdate nama file wajah ke database
def update_database_with_recognized_faces(recognized_names):
    try:
        connection = mysql.connector.connect(
            host='localhost',  # Sesuaikan dengan host MySQL Anda
            user='root',  # Sesuaikan dengan username MySQL Anda
            password='',  # Sesuaikan dengan password MySQL Anda
            database='janimani'  # Sesuaikan dengan nama database MySQL Anda
        )
        
        cursor = connection.cursor()
        
        if recognized_names:
            if not any(name and name != "Unknown" for name, _ in recognized_names):
                # Insert 0 into qrcode table if no face is recognized or if the face is unknown
                query = "INSERT INTO qrcode (kode) VALUES (0)"
                cursor.execute(query)
                connection.commit()
                print("Inserted 0 into qrcode table")
            else:
                for name, filename in recognized_names:
                    if name and name != "Unknown":
                        # Insert into the qrcode table
                        query = "INSERT INTO qrcode (kode) VALUES (%s)"
                        cursor.execute(query, (filename,))
                        connection.commit()
                        print(f"Inserted '{filename}' into qrcode table")
                        break  # Insert only once
        else:
            print("No recognized names to update in the database.")
        
        print("Database updated successfully.")
    except mysql.connector.Error as error:
        print("Failed to update record to database: {}".format(error))
    finally:
        if connection.is_connected():
            cursor.close()
            connection.close()
            print("MySQL connection is closed")

# Folder yang berisi gambar-gambar yang akan dikenali
faces_to_recognize_folder = "C:/xampp/htdocs/janimaniabsensi/img/"
gagal_folder = "C:/xampp/htdocs/janimaniabsensi/gagal/"
scan_folder = "C:/xampp/htdocs/janimaniabsensi/scan/"

# Create gagal and scan folders if they do not exist
if not os.path.exists(gagal_folder):
    os.makedirs(gagal_folder)
if not os.path.exists(scan_folder):
    os.makedirs(scan_folder)

try:
    while True:
        mode = get_mode_from_database()
        image_files = os.listdir(faces_to_recognize_folder)
        
        if mode == 1:
            if image_files:  # Cek jika folder tidak kosong
                recognized_names = recognize_faces_from_folder(faces_to_recognize_folder, gagal_folder, scan_folder)
                if recognized_names:
                    print("Recognized faces:")
                    for name, filename in recognized_names:
                        if name is not None:
                            print(name)
                    update_database_with_recognized_faces(recognized_names)
                else:
                    print("No faces recognized. Reloading...")
                    update_database_with_recognized_faces([])  # Update database if no faces found but images exist
            else:
                print("Folder img is empty. Skipping face recognition and database update.")
        elif mode == 0:
            print("Mode is 0. Skipping face recognition and file moving.")
        
        time.sleep(5)  # Tunggu 5 detik sebelum mencoba lagi
except KeyboardInterrupt:
    print("Program dihentikan oleh pengguna.")
