# Serialisierte Sitzungsdaten begrenzen / Core 0.68

Die Größe des erzeugten Bundles kann jetzt direkt nach SERIALIZING und vor REPLAYING begrenzt werden:

    ./build/bruce grid-batch --paths '2,6;4,3' \
      --verification replay --max-serialized-bytes 100000

Die Grenze ist inklusiv. Ein Bundle mit exakt dieser Größe wird verarbeitet. Wird sie überschritten, endet die Sitzung in \`SERIALIZING\` mit \`SerializationByteBudgetExceeded\`; es gibt keinen Replayversuch und keinen Ergebnisexport.

Der Erfolgsbericht verwendet \`application-execution-v15\` und enthält:

- \`max_serialized_bytes\`
- \`serialized_bytes\`

Der Fehlerbericht verwendet bei aktivem Budget \`application-execution-failure-v3\` und enthält dieselben Angaben. Eine vorhandene Zieldatei bleibt bei einem Fehler unverändert.

Das Budget ist von \`--max-replay-bytes\` und \`--max-session-checkpoints\` getrennt. Es zählt serialisierte Bundlebytes, keinen Prozessspeicher, keine CPU-Zeit und keine mathematischen Operationen. Der Payloadaufbau bis zur Größenprüfung bleibt atomar; vorhandene innere Serialisierungspunkte bleiben unabhängig bestehen.

[Vertrag](141-serialized-byte-budget-contract.md) · [Abnahmematrix](136-m0-m2-acceptance-matrix.md)
