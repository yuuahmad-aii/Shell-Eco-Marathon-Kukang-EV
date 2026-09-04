// Import the functions you need from the SDKs you need
import { initializeApp } from "firebase/app";
import { getDatabase, ref, onValue } from "firebase/database";

// Your web app's Firebase configuration
const firebaseConfig = {
    apiKey: "AIzaSyCkwhZaXiJ1YkHU792jhgdiCZVIZLXcLu0",
    authDomain: "kukang-ev.firebaseapp.com",
    databaseURL: "https://kukang-ev-default-rtdb.asia-southeast1.firebasedatabase.app",
    projectId: "kukang-ev",
    storageBucket: "kukang-ev.firebasestorage.app",
    messagingSenderId: "845116041519",
    appId: "1:845116041519:web:0649aa2f245581965d19da",
    measurementId: "G-HFCB7XSV4S"
};

// Initialize Firebase
const app = initializeApp(firebaseConfig);
const db = getDatabase(app);

export { db, ref, onValue };
