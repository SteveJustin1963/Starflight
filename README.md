# Starflight
Starflight is a classic game that was first published in 1986 by Binary Systems. My code is a small MINT version of it. 

my take on game using MINT code and out put to 3d globe, circular and xyz persistence displays to enhance gameplay. synthesizer chips to make interesting sounds. 

```
BEGIN GAME

// Initialization
SET solar_system = [] // Rename galaxy to solar_system
SET max_planets = 20 // Solar system with 20 planets
SET current_planet = NULL
SET current_landscape = NULL
SET game_over = FALSE

// Generate Solar System with Planets
DISPLAY "Generating a new solar system..."
FOR i = 1 TO max_planets DO
    planet = {name: NULL, landscape: NULL}
    solar_system.APPEND(planet)
END FOR
DISPLAY max_planets, " planets created in the solar system!"

// Main Game Loop
WHILE game_over == FALSE DO
    DISPLAY "Options: [1] View Solar System [2] Choose Planet [3] Exit Game"
    INPUT player_choice

    IF player_choice == 1 THEN
        DISPLAY "Solar System Overview:"
        FOR i = 1 TO LENGTH(solar_system) DO
            IF solar_system[i].name != NULL THEN
                DISPLAY i, ": ", solar_system[i].name
            ELSE
                DISPLAY i, ": Unnamed Planet"
            END IF
        END FOR

    ELSE IF player_choice == 2 THEN
        DISPLAY "Choose a planet (1-", LENGTH(solar_system), "):"
        INPUT planet_choice
        current_planet = solar_system[planet_choice]

        IF current_planet.name == NULL THEN
            DISPLAY "This planet is unnamed. Generate a name for it!"
            current_planet.name = NAME_PLANET()
        END IF

        DISPLAY "Flying to planet: ", current_planet.name
        DISPLAY "Generating landscape for ", current_planet.name, "..."
        IF current_planet.landscape == NULL THEN
            current_planet.landscape = GENERATE_LANDSCAPE(current_planet.name)
        END IF

        DISPLAY "Arrived at ", current_planet.name, "!"
        DISPLAY "Options: [1] Explore Planet [2] Back to Solar System"
        INPUT planet_action

        IF planet_action == 1 THEN
            current_landscape = current_planet.landscape
            EXPLORE_LANDSCAPE(current_landscape)
        END IF

    ELSE IF player_choice == 3 THEN
        game_over = TRUE
        DISPLAY "Exiting Game. Goodbye!"

    ELSE
        DISPLAY "Invalid Choice. Try Again!"
    END IF
END WHILE

END GAME

// Function Definitions

FUNCTION NAME_PLANET()
    DISPLAY "Enter 8 characters to create planet name anagrams:"
    INPUT base_name

    WHILE LENGTH(base_name) != 8 DO
        DISPLAY "Name must be 8 characters. Try again."
        INPUT base_name
    END WHILE

    anagram_names = GENERATE_ANAGRAMS(base_name)
    DISPLAY "Generated Names:"
    FOR i = 1 TO LENGTH(anagram_names) DO
        DISPLAY i, ": ", anagram_names[i]
    END FOR

    DISPLAY "Choose a name (1-", LENGTH(anagram_names), "):"
    INPUT choice
    CHOSEN_NAME = anagram_names[choice]

    // Check for duplicate names
    WHILE IS_DUPLICATE(CHOSEN_NAME, solar_system) DO
        DISPLAY "Name already exists. Please choose another."
        INPUT choice
        CHOSEN_NAME = anagram_names[choice]
    END WHILE

    RETURN CHOSEN_NAME
END FUNCTION

FUNCTION GENERATE_ANAGRAMS(base_name)
    anagrams = []
    WHILE LENGTH(anagrams) < 20 DO
        new_anagram = RANDOM_ANAGRAM(base_name)
        IF new_anagram NOT IN anagrams THEN
            anagrams.APPEND(new_anagram)
        END IF
    END WHILE
    RETURN anagrams
END FUNCTION

FUNCTION IS_DUPLICATE(name, solar_system)
    FOR planet IN solar_system DO
        IF planet.name == name THEN
            RETURN TRUE
        END IF
    END FOR
    RETURN FALSE
END FUNCTION

FUNCTION GENERATE_LANDSCAPE(planet_name)
    DISPLAY "Creating fractal-based landscape for ", planet_name, "..."
    fractal_seed = RANDOM_SEED()
    landscape = FRACTAL_RENDER(fractal_seed)
    RETURN landscape
END FUNCTION

FUNCTION EXPLORE_LANDSCAPE(landscape)
    DISPLAY "Exploring the landscape of this planet..."
    // Simplified exploration logic
    FOR i = 1 TO RANDOM(5, 10) DO
        DISPLAY "You discover something interesting!"
    END FOR
    DISPLAY "Exploration complete."
END FUNCTION
```

Here’s a step-by-step walkthrough of how the game progresses based on the pseudocode:

---

### **Walkthrough: Solar System Game**

---

#### **1. Game Initialization**
- **Action**: The game starts by creating a solar system with 20 unnamed planets.
- **Output**:
  ```plaintext
  Generating a new solar system...
  20 planets created in the solar system!
  ```

---

#### **2. Main Menu Options**
- The user sees three main options:
  ```plaintext
  Options: 
  [1] View Solar System 
  [2] Choose Planet 
  [3] Exit Game
  ```
- The user selects an option.

---

#### **3. Viewing the Solar System**
- **If Option 1 is chosen**:
  - The game displays the solar system, showing which planets are named and which are still unnamed.
  - **Example Output**:
    ```plaintext
    Solar System Overview:
    1: Unnamed Planet
    2: Unnamed Planet
    3: Planet Zorath
    4: Unnamed Planet
    ...
    ```

---

#### **4. Choosing a Planet**
- **If Option 2 is chosen**:
  - The game asks the user to pick a planet:
    ```plaintext
    Choose a planet (1-20):
    ```
  - The user selects a number, e.g., `2`.

---

#### **5. Naming a Planet**
- If the selected planet is unnamed, the game prompts the user to name it:
  ```plaintext
  This planet is unnamed. Generate a name for it!
  Enter 8 characters to create planet name anagrams:
  ```
- The user enters a word, e.g., `SOLARFUN`.

---

#### **6. Generating Anagrams**
- The game generates 20 anagrams of the input and displays them:
  ```plaintext
  Generated Names:
  1: FUNOSRAL
  2: SOLARFUN
  3: RONUSALF
  4: UNSOLRAF
  ...
  ```
- The user chooses one, e.g., `2` (SOLARFUN).

- The game checks for duplicates:
  - If it’s a duplicate, the user is prompted to choose another.
  - If it’s unique, the name is assigned to the planet:
    ```plaintext
    Planet 2 is now named SOLARFUN!
    ```

---

#### **7. Flying to the Planet**
- The game generates a fractal-based landscape for the planet (if it hasn’t been generated yet):
  ```plaintext
  Flying to planet: SOLARFUN
  Generating landscape for SOLARFUN...
  Landscape generated!
  Arrived at SOLARFUN!
  ```
- The user sees two options:
  ```plaintext
  Options: 
  [1] Explore Planet 
  [2] Back to Solar System
  ```
- If they choose to explore, the game enters the exploration phase.

---

#### **8. Exploring the Planet’s Landscape**
- **If Option 1 is chosen**:
  - The game simulates exploration by randomly generating discoveries.
  - **Example Output**:
    ```plaintext
    Exploring the landscape of this planet...
    You discover an ancient ruin!
    You find rare minerals!
    You encounter alien wildlife!
    Exploration complete.
    ```
- The user is returned to the planet menu.

---

#### **9. Returning to the Main Menu**
- **If Option 2 is chosen (or exploration is complete)**:
  - The user is returned to the main menu to view the solar system or choose another planet.

---

#### **10. Exiting the Game**
- **If Option 3 is chosen**:
  ```plaintext
  Exiting Game. Goodbye!
  ```

---

### **Example Walkthrough**
Here’s a sample session:

1. **Game Start**:
   ```plaintext
   Generating a new solar system...
   20 planets created in the solar system!
   ```

2. **View Solar System**:
   ```plaintext
   Options: 
   [1] View Solar System 
   [2] Choose Planet 
   [3] Exit Game
   ```
   - User selects `[1]`.

   ```plaintext
   Solar System Overview:
   1: Unnamed Planet
   2: Unnamed Planet
   3: Planet Zorath
   ...
   ```

3. **Choose Planet**:
   - User selects `[2]` and chooses Planet 2.

   ```plaintext
   This planet is unnamed. Generate a name for it!
   Enter 8 characters to create planet name anagrams:
   ```
   - User enters `SOLARFUN`.

   ```plaintext
   Generated Names:
   1: FUNOSRAL
   2: SOLARFUN
   3: RONUSALF
   ...
   Choose a name (1-20):
   ```
   - User selects `[2]`.

   ```plaintext
   Planet 2 is now named SOLARFUN!
   ```

4. **Fly to Planet**:
   ```plaintext
   Flying to planet: SOLARFUN
   Generating landscape for SOLARFUN...
   Landscape generated!
   Arrived at SOLARFUN!
   Options: 
   [1] Explore Planet 
   [2] Back to Solar System
   ```
   - User selects `[1]`.

5. **Explore Landscape**:
   ```plaintext
   Exploring the landscape of this planet...
   You discover an ancient ruin!
   You find rare minerals!
   You encounter alien wildlife!
   Exploration complete.
   ```

6. **Back to Main Menu**:
   - User returns to the main menu and selects another planet or exits.

---

 


