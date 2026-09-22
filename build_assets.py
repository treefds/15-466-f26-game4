import shutil
import os

if __name__ == "__main__":
    shutil.copytree("assets/font", "dist/font", dirs_exist_ok=True)
    shutil.copytree("assets/images", "dist/images", dirs_exist_ok=True)
    shutil.copytree("assets/sounds", "dist/sounds", dirs_exist_ok=True)
    shutil.copy("assets/script.txt", "dist/script.txt")
