#include <Arduino_FreeRTOS.h>
#include <semphr.h>
#include <Servo.h>

// Pines
#define GAZ_SENSOR A0
#define FLAME_SENSOR 4
#define IR_SENSOR_IN 2
#define IR_SENSOR_OUT 3
#define LED_ROSU 7
#define BUZZER 8
#define BUTON_URGENTA 6
#define BUTON_RESET 5
#define SERVO_IN 26
#define SERVO_OUT 27
#define LED_ALB 39
#define LED_GALBEN 38
#define FOTO_SENSOR 40

// Variabile globale
volatile int nrMasini = 0;
volatile bool stareUrgenta = false;
volatile bool stareUrgentaAnuntata = false;
Servo servoIn;
Servo servoOut;
SemaphoreHandle_t mutexNrMasini;
SemaphoreHandle_t mutexStareUrgenta;

// Prototipuri de functii
void TaskStareUrgenta(void *pvParameters);
void TaskGaz(void *pvParameters);
void TaskFoc(void *pvParameters);
void TaskNrMasini(void *pvParameters);
void TaskBarieraIn(void *pvParameters);
void TaskBarieraOut(void *pvParameters);
void TaskButonPericol(void *pvParameters);
void TaskButonReset(void *pvParameters);
void TaskSerialPericol(void *pvParameters);
void TaskLumina(void *pvParameters);

void setup() {
  Serial.begin(115200);

  // Initializare pini
  pinMode(GAZ_SENSOR, INPUT);
  pinMode(FLAME_SENSOR, INPUT);
  pinMode(IR_SENSOR_IN, INPUT);
  pinMode(IR_SENSOR_OUT, INPUT);
  pinMode(LED_ROSU, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(BUTON_URGENTA, INPUT_PULLUP);
  pinMode(BUTON_RESET, INPUT_PULLUP);
  pinMode(LED_ALB, OUTPUT); // LED alb pentru zi
  pinMode(LED_GALBEN, OUTPUT); // LED galben pentru noapte
  pinMode(FOTO_SENSOR, INPUT); // Senzor fotosensibil

  servoIn.attach(SERVO_IN);
  servoOut.attach(SERVO_OUT);
  servoIn.write(0);  // Bariera deschisa
  servoOut.write(0); // Bariera deschisa

  // Creare mutex-uri
  mutexNrMasini = xSemaphoreCreateMutex();
  mutexStareUrgenta = xSemaphoreCreateMutex();

  // Creare task-uri
  xTaskCreate(TaskStareUrgenta, "StareUrgenta", 128, NULL, 2, NULL);
  xTaskCreate(TaskGaz, "Gaz", 128, NULL, 1, NULL);
  xTaskCreate(TaskFoc, "Foc", 128, NULL, 1, NULL);
  xTaskCreate(TaskNrMasini, "NrMasini", 128, NULL, 1, NULL);
  xTaskCreate(TaskBarieraIn, "BarieraIn", 128, NULL, 1, NULL);
  xTaskCreate(TaskBarieraOut, "BarieraOut", 128, NULL, 1, NULL);
  xTaskCreate(TaskButonPericol, "ButonPericol", 128, NULL, 1, NULL);
  xTaskCreate(TaskButonReset, "ButonReset", 128, NULL, 1, NULL);
  xTaskCreate(TaskSerialPericol, "SerialPericol", 128, NULL, 1, NULL);
  xTaskCreate(TaskLumina, "Lumina", 128, NULL, 1, NULL); 
  vTaskStartScheduler();
}

void loop() {}

void TaskStareUrgenta(void *pvParameters) {
  while (1) {
    xSemaphoreTake(mutexStareUrgenta, portMAX_DELAY);
     if (stareUrgenta && !stareUrgentaAnuntata) {
      Serial.println("Am intrat in starea de urgenta!");
      stareUrgentaAnuntata = true;
    }
     if (!stareUrgenta && stareUrgentaAnuntata) {
      Serial.println("Am iesit din starea de urgenta!");
      stareUrgentaAnuntata = false;
    }
    if (stareUrgenta) {
      digitalWrite(LED_ROSU, HIGH);
      // Activează buzzer-ul cu un ton intermitent
      digitalWrite(BUZZER, HIGH);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      digitalWrite(BUZZER, LOW);
      vTaskDelay(500 / portTICK_PERIOD_MS);
    } else {
      digitalWrite(LED_ROSU, LOW);
      digitalWrite(BUZZER, LOW);
    }
    xSemaphoreGive(mutexStareUrgenta);
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}


void TaskGaz(void *pvParameters) {
  while (1) {
    int valoareGaz = analogRead(GAZ_SENSOR);
    if (valoareGaz > 400) { // Prag pentru detectie gaz
      Serial.println("S-a detectat scurgeri de gaz!");
      xSemaphoreTake(mutexStareUrgenta, portMAX_DELAY);
      stareUrgenta = true;
      xSemaphoreGive(mutexStareUrgenta);
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

void TaskFoc(void *pvParameters) {
  while (1) {
    int valoareFlacara = digitalRead(FLAME_SENSOR);
    if (valoareFlacara == LOW) { // Detectie flacara
      Serial.println("S-a detectat foc!");
      xSemaphoreTake(mutexStareUrgenta, portMAX_DELAY);
      stareUrgenta = true;
      xSemaphoreGive(mutexStareUrgenta);
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

void TaskNrMasini(void *pvParameters) {
  while (1) {
    // Dacă este stare de urgență, nu lăsăm mașinile să intre
    if (stareUrgenta) {
      // În stare de urgență, nu se permite intrarea unei mașini
      if (digitalRead(IR_SENSOR_IN) == LOW) {
        Serial.println("Intrarea este blocată! Stare de urgenta!");
        vTaskDelay(500 / portTICK_PERIOD_MS); // Evităm citirile prea frecvente
      }
    } else {
      // Dacă nu este stare de urgență, lăsăm mașinile să intre
      if (digitalRead(IR_SENSOR_IN) == LOW) {
        xSemaphoreTake(mutexNrMasini, portMAX_DELAY);
        nrMasini++;
        Serial.print("Masina intrata. Numar total: ");
        Serial.println(nrMasini);
        xSemaphoreGive(mutexNrMasini);
        vTaskDelay(500 / portTICK_PERIOD_MS); // Debounce
      }
    }

    // Logica pentru ieșirea mașinilor rămâne neschimbată
    if (digitalRead(IR_SENSOR_OUT) == LOW) {
      xSemaphoreTake(mutexNrMasini, portMAX_DELAY);
      if (nrMasini > 0) nrMasini--;
      Serial.print("Masina iesita. Numar total: ");
      Serial.println(nrMasini);
      xSemaphoreGive(mutexNrMasini);
      vTaskDelay(500 / portTICK_PERIOD_MS); // Debounce
    }
  }
}



void TaskBarieraIn(void *pvParameters) {
  while (1) {
    xSemaphoreTake(mutexStareUrgenta, portMAX_DELAY);
    if (stareUrgenta) {
      servoIn.write(90); // Bariera inchisa
    } else {
      servoIn.write(0); // Bariera deschisa
    }
    xSemaphoreGive(mutexStareUrgenta);
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void TaskBarieraOut(void *pvParameters) {
  while (1) {
    xSemaphoreTake(mutexStareUrgenta, portMAX_DELAY);
    if (stareUrgenta) {
      xSemaphoreTake(mutexNrMasini, portMAX_DELAY);
      if (nrMasini == 0) {
        servoOut.write(90); // Bariera inchisa
      }
      xSemaphoreGive(mutexNrMasini);
    } else {
      servoOut.write(0); // Bariera deschisa
    }
    xSemaphoreGive(mutexStareUrgenta);
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void TaskButonPericol(void *pvParameters) {
  while (1) {
    if (digitalRead(BUTON_URGENTA) == LOW) {
      Serial.println("S-a apasat butonul de urgenta!");
      xSemaphoreTake(mutexStareUrgenta, portMAX_DELAY);
      stareUrgenta = true;
      xSemaphoreGive(mutexStareUrgenta);
      vTaskDelay(500 / portTICK_PERIOD_MS); // Debounce
    }
  }
}

void TaskButonReset(void *pvParameters) {
  while (1) {
    if (digitalRead(BUTON_RESET) == LOW) {
      xSemaphoreTake(mutexStareUrgenta, portMAX_DELAY);
      stareUrgenta = false;
      xSemaphoreGive(mutexStareUrgenta);
      vTaskDelay(500 / portTICK_PERIOD_MS); // Debounce
    }
  }
}

void TaskSerialPericol(void *pvParameters) {
  while (1) {
    if (Serial.available() > 0) {
      String input = Serial.readStringUntil('\n'); // Citește linia din Serial Monitor
      input.trim(); // Elimină spațiile albe sau caracterele de final
      if (input.equalsIgnoreCase("PERICOL")) {
        Serial.println("Comanda PERICOL detectata! Intram in starea de urgenta.");
        xSemaphoreTake(mutexStareUrgenta, portMAX_DELAY);
        stareUrgenta = true;
        xSemaphoreGive(mutexStareUrgenta);
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS); // Interval mic pentru a evita suprasolicitarea procesorului
  }
}

// Task pentru gestionarea luminii de zi/noapte
void TaskLumina(void *pvParameters) {
  int lastLuminaValue = -1; // Inițializare cu o valoare care nu este valabilă (nu poate fi HIGH sau LOW)

  while (1) {
    // Citim valoarea de la senzorul digital de lumină
    int valoareLumina = digitalRead(FOTO_SENSOR);

    // Verificăm dacă valoarea s-a schimbat
    if (valoareLumina != lastLuminaValue) {
      // Dacă valoarea s-a schimbat, afișăm în Serial Monitor
      Serial.print("Valoare lumina: ");
      Serial.println(valoareLumina);

      // Actualizăm valoarea anterioară
      lastLuminaValue = valoareLumina;

      // Dacă senzorul detectează lumină suficientă (HIGH)
      if (valoareLumina == LOW) {
        Serial.println("Este ziua!");
        digitalWrite(LED_ALB, HIGH); // Aprindem LED-ul alb
        digitalWrite(LED_GALBEN, LOW); // Stingem LED-ul galben
      } else { // Dacă nu este lumină suficientă (LOW)
        Serial.println("Este noaptea!");
        digitalWrite(LED_ALB, LOW); // Stingem LED-ul alb
        digitalWrite(LED_GALBEN, HIGH); // Aprindem LED-ul galben
      }
    }

    vTaskDelay(500 / portTICK_PERIOD_MS); // Așteptăm un interval pentru a evita citirile prea frecvente
  }
}
