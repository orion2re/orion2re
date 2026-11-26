#pragma once

namespace FILEDEF {
    /* Loads ORION2RE.SET into the global settings state and reapplies the saved sound volume. */
    void Load_Game_Settings();
    /* Saves the current global settings state to ORION2RE.SET. */
    bool Save_Game_Settings(void);
    /* Updates the persisted settings version, or resets defaults if the settings file cannot be rewritten. */
    void Update_Game_Settings_Version_Number();

    /* Writes the current game state to a zero-based save slot.
     * slot_index: Save slot index to write.
     */
    bool __cdecl Save_Game(int16_t slot_index);
    /** Writes an already-serialized autosave without running UI or network callbacks. */
    bool Write_Autosave(const void* data, size_t size);
    /* Loads a saved game from a zero-based save slot.
     * slot_index: Save slot index to read.
     */
    uint32_t __cdecl Load_Game(int16_t slot_index);
    extern const char s_Auto_Save_00556938[12];
    /* Reads the saved game type byte from a zero-based save slot.
     * save_slot: Save slot index to inspect.
     */
    int8_t __cdecl Get_Game_Type_From_Saved_Game(int16_t save_slot);
    /* Stub hook for writing custom player data into the save stream.
     * player: Player record to serialize.
     */
    void __cdecl Save_Moo_Custom_Player_Data(s_player* player);
    /* Populates save slot descriptions and stardates for the save/load UI.
     * save_times: Receives per-slot stardates, or -1 for invalid or missing saves.
     */
    void __cdecl Get_Saved_Game_Descriptions(int32_t* save_times);
    /* Stub hook for reading custom player data from the save stream.
     * player: Player record to populate.
     */
    bool __cdecl Read_Moo_Custom_Player_Data(s_player* player);
} // namespace FILEDEF
