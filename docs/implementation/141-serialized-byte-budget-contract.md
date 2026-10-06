# Sitzungs-Bundlegröße vor Replay begrenzen / Core 0.68

`VerificationBudget::max_serialized_bytes` begrenzt das erzeugte Bundle direkt nach SERIALIZING und vor dem Übergang zu REPLAYING. Das Budget ist von `max_replay_bytes` (Eingangsgrenze beim Replay) und vom gemeinsamen Prüfpunktbudget getrennt.

Die Grenze ist inklusiv: Ein Bundle mit exakt dieser Größe wird angenommen. Überschreitung erzeugt `SerializationByteBudgetExceeded` in SERIALIZING, keinen Replayversuch und keinen exportierbaren Sitzungserfolg. Der Fehlerbericht enthält Limit und tatsächlich erzeugte Bundlegröße.

Die Grenze misst serialisierte Bytes, nicht Speicherverbrauch, CPU-Zeit, mathematische Operationen oder Dateisystemgröße. Der Payloadaufbau selbst bleibt bis zu dieser Prüfung atomar; bereits laufende innere Serialisierungspunkte und deren Abbruchmodi bleiben separat.
