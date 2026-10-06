#!/usr/bin/env python3
"""Run the real CLI, summarize it, and verify the exported run in a new process."""
import json
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
binary = root / 'build' / 'bruce'
if not binary.is_file():
    raise SystemExit('Bitte zuerst bauen: cmake -G Ninja -S . -B build && cmake --build build')

with tempfile.TemporaryDirectory(prefix='bruce-demo-') as directory:
    bundle = Path(directory) / 'run.bruce'
    run = json.loads(subprocess.check_output([
        str(binary), 'research', '--train', str(root/'examples/affine-training.csv'),
        '--holdout', str(root/'examples/affine-holdout.csv'), '--output', str(bundle)
    ], text=True))
    print('BRUCE — erster mathematischer Forschungsablauf')
    print('Lernpaare: (0,3), (1,5), (4,11)')
    print('Zurückgehaltene Prüfpaare: (2,7), (10,23)')
    print('Dynamisch untersuchte Kandidaten:', run['candidates_executed'])
    for hypothesis in run['hypotheses']:
        print('Gefundene Formel:', hypothesis['formula'])
    print('Status:', run['status'])
    print(run['explanation'])
    repeated = json.loads(subprocess.check_output([str(binary), 'replay', str(bundle)], text=True))
    if repeated != run:
        raise SystemExit('Replay stimmt nicht mit dem ursprünglichen Lauf überein.')
    print('Replay in einem neuen Prozess: identischer Befund und identische Prüfspur.')
    selection = json.loads(subprocess.check_output([
        str(binary), 'compatibility', '--rules', str(root/'examples/research-selection.rules'),
        '--from-run', str(bundle), '--at', '10'
    ], text=True))
    print('Forschung → geladene Regeln → Auswahl:', selection['status'])
    print('Mathematischer Ursprungsstatus bleibt:', selection['origin_status'])
    filters = json.loads(subprocess.check_output([
        str(binary), 'compatibility', '--rules', str(root/'examples/compatibility.rules'),
        '--candidates', str(root/'examples/compatibility-candidates.csv')
    ], text=True))
    print('Separater Filterfall:', filters['status'], 'gleichrangige IDs:', ', '.join(filters['selected_ids']))
    refuted = json.loads(subprocess.check_output([
        str(binary), 'research', '--train', str(root/'examples/affine-training.csv'),
        '--holdout', str(root/'examples/affine-counterexample.csv')
    ], text=True))
    print('Gegenbeispiel (10,24):', refuted['status'])
    exact = json.loads(subprocess.check_output([
        str(binary), 'calculate', '--domain', 'rational', '--op', 'add',
        '--left', '1/3', '--right', '1/6'
    ], text=True))
    print('Exakte Bruchrechnung: 1/3 + 1/6 =', exact['value'])
    rational_bundle = Path(directory) / 'rational.bruce'
    rational = json.loads(subprocess.check_output([
        str(binary), 'research', '--domain', 'rational',
        '--train', str(root/'examples/rational-training.csv'),
        '--holdout', str(root/'examples/rational-holdout.csv'),
        '--min', '-2', '--max', '2', '--max-denominator', '3',
        '--output', str(rational_bundle)
    ], text=True))
    print('Rationale Forschung:', rational['hypotheses'][0]['formula'], '→', rational['status'])
    n = 10**100
    large_training = Path(directory) / 'large-training.csv'
    large_training.write_text(f'x,y\n{n},{2*n+3}\n{n+1},{2*(n+1)+3}\n')
    large_holdout = Path(directory) / 'large-holdout.csv'
    large_holdout.write_text(f'x,y\n{n+9},{2*(n+9)+3}\n')
    large_bundle = Path(directory) / 'large.bruce'
    large = json.loads(subprocess.check_output([
        str(binary), 'research', '--domain', 'bigint', '--train', str(large_training),
        '--holdout', str(large_holdout), '--output', str(large_bundle)
    ], text=True))
    print('Forschung mit Eingaben ab 10^100:', large['hypotheses'][0]['formula'], '→', large['status'])
    large_training.unlink(); large_holdout.unlink()
    for archived, expected in [(rational_bundle, rational), (large_bundle, large)]:
        replayed = json.loads(subprocess.check_output([str(binary), 'replay', str(archived)], text=True))
        if replayed != expected:
            raise SystemExit('Replay der neuen Zahlendomäne stimmt nicht überein.')
    print('Beide neuen Zahlendomänen: identisches Replay, auch ohne ursprüngliche BigInt-Eingabedateien.')
    power_bundle = Path(directory) / 'power.bruce'
    power = json.loads(subprocess.check_output([
        str(binary), 'research', '--strategy', 'power-affine',
        '--train', str(root/'examples/power-training.csv'),
        '--holdout', str(root/'examples/power-holdout.csv'), '--output', str(power_bundle)
    ], text=True))
    print('Nichtlineare Forschung:', power['hypotheses'][0]['formula'], '→', power['status'])
    print('Selbst untersuchte Exponenten: 1 bis', power['max_power'],
          '– tatsächlich geprüfte Kandidaten:', power['candidates_executed'])
    if json.loads(subprocess.check_output([str(binary), 'replay', str(power_bundle)], text=True)) != power:
        raise SystemExit('Replay der nichtlinearen Forschung stimmt nicht überein.')
    power_selection = json.loads(subprocess.check_output([
        str(binary), 'compatibility', '--rules', str(root/'examples/power-selection.rules'),
        '--from-run', str(power_bundle), '--at', '-3'
    ], text=True))
    print('Nichtlineare Formel bei x=-3 →', power_selection['candidates'][0]['value'],
          '→ Regelauswahl:', power_selection['status'])
    power_bad = json.loads(subprocess.check_output([
        str(binary), 'research', '--strategy', 'power-affine',
        '--train', str(root/'examples/power-training.csv'),
        '--holdout', str(root/'examples/power-counterexample.csv')
    ], text=True))
    print('Nichtlineares Gegenbeispiel (3,99):', power_bad['status'])
    adaptive_bundle = Path(directory) / 'adaptive.bruce'
    adaptive = json.loads(subprocess.check_output([
        str(binary), 'research', '--strategy', 'power-affine', '--planner', 'adaptive',
        '--train', str(root/'examples/power-training.csv'),
        '--holdout', str(root/'examples/power-holdout.csv'), '--output', str(adaptive_bundle)
    ], text=True))
    if (adaptive['status'], adaptive['hypotheses']) != (power['status'], power['hypotheses']):
        raise SystemExit('Adaptiver Planer verändert den fachlichen Befund.')
    print('Adaptive Planung: gleicher Befund und gleiche Kandidaten-IDs.')
    print('Ausgeführte Kandidaten:', power['candidates_executed'], '→', adaptive['candidates_executed'])
    print('Logische Operationen einschließlich Planung:', power['operations_used'], '→', adaptive['operations_used'])
    if json.loads(subprocess.check_output([str(binary), 'replay', str(adaptive_bundle)], text=True)) != adaptive:
        raise SystemExit('Replay der adaptiven Forschung stimmt nicht überein.')
    print('Auch die Planungsentscheidungen sind identisch replaybar.')
    polynomial_bundle = Path(directory) / 'polynomial.bruce'
    polynomial = json.loads(subprocess.check_output([
        str(binary), 'research', '--strategy', 'polynomial',
        '--train', str(root/'examples/polynomial-training.csv'),
        '--holdout', str(root/'examples/polynomial-holdout.csv'), '--output', str(polynomial_bundle)
    ], text=True))
    print('Mehrtermige Polynomforschung:', polynomial['hypotheses'][0]['formula'], '→', polynomial['status'])
    if json.loads(subprocess.check_output([str(binary), 'replay', str(polynomial_bundle)], text=True)) != polynomial:
        raise SystemExit('Replay der Polynomforschung stimmt nicht überein.')
    projected = json.loads(subprocess.check_output([
        str(binary), 'compatibility', '--rules', str(root/'examples/polynomial-selection.rules'),
        '--from-run', str(polynomial_bundle), '--at', '-3'
    ], text=True))
    print('Polynom bei x=-3 →', projected['candidates'][0]['value'], '→', projected['status'])
    bad = json.loads(subprocess.check_output([
        str(binary), 'research', '--strategy', 'polynomial',
        '--train', str(root/'examples/polynomial-training.csv'),
        '--holdout', str(root/'examples/polynomial-counterexample.csv')
    ], text=True))
    print('Polynom-Gegenbeispiel (4,99):', bad['status'])
    portfolio_bundle = Path(directory) / 'portfolio.bruce'
    portfolio = json.loads(subprocess.check_output([
        str(binary), 'research', '--strategy', 'portfolio',
        '--train', str(root/'examples/portfolio-training.csv'),
        '--holdout', str(root/'examples/power-holdout.csv'), '--output', str(portfolio_bundle)
    ], text=True))
    print('Gemeinsamer Forschungslauf:', portfolio['status'])
    print('Ausgeführte Strategien:', ', '.join(r['strategy'] for r in portfolio['strategy_runs']))
    print('Zusammengeführte Formel:', portfolio['hypotheses'][0]['formula'])
    print('Erhaltene Herkünfte:', ', '.join(o['strategy'] for o in portfolio['hypotheses'][0]['origins']))
    print('Gemeinsame Operationseinheiten:', portfolio['operations_used'])
    if json.loads(subprocess.check_output([str(binary), 'replay', str(portfolio_bundle)], text=True)) != portfolio:
        raise SystemExit('Replay der gemeinsamen Strategieplanung stimmt nicht überein.')
    print('Gleiche Eingaben zählen dabei nicht als zusätzliche unabhängige Evidenz.')
    fraction_bundle = Path(directory) / 'fraction.bruce'
    fraction_args = [str(binary), 'research', '--strategy', 'rational-linear',
                     '--train', str(root/'examples/fraction-training.csv')]
    fraction = json.loads(subprocess.check_output(fraction_args + [
        '--holdout', str(root/'examples/fraction-holdout.csv'), '--output', str(fraction_bundle)
    ], text=True))
    if fraction['status'] != 'VERIFIED_ON_SAMPLES':
        raise SystemExit('Bruchfunktion besteht Hold-out nicht.')
    print('Rationale Bruchfunktion:', fraction['hypotheses'][0]['formula'], '→', fraction['status'])
    print('Datenabhängig abgeleitete Kandidaten:', fraction['candidates_executed'])
    if json.loads(subprocess.check_output([str(binary), 'replay', str(fraction_bundle)], text=True)) != fraction:
        raise SystemExit('Replay der Bruchfunktion stimmt nicht überein.')
    pole = json.loads(subprocess.check_output(fraction_args + [
        '--holdout', str(root/'examples/fraction-pole.csv')
    ], text=True))
    if pole['status'] != 'REFUTED':
        raise SystemExit('Nennernullstelle wurde nicht erkannt.')
    print('Erwartete endliche Ausgabe an der Nennernullstelle x=-1:', pole['status'])
    fraction_selection = json.loads(subprocess.check_output([
        str(binary), 'compatibility', '--rules', str(root/'examples/fraction-selection.rules'),
        '--from-run', str(fraction_bundle), '--at', '0'
    ], text=True))
    print('Bruchfunktion bei x=0 →', fraction_selection['candidates'][0]['value'], '→', fraction_selection['status'])
    combined_bundle = Path(directory) / 'rational-portfolio.bruce'
    combined = json.loads(subprocess.check_output([
        str(binary), 'research', '--strategy', 'portfolio-rational',
        '--train', str(root/'examples/fraction-training.csv'),
        '--holdout', str(root/'examples/fraction-holdout.csv'), '--max-degree', '2',
        '--output', str(combined_bundle)
    ], text=True))
    if combined['status'] != 'VERIFIED_ON_SAMPLES' or len(combined['strategy_runs']) != 3:
        raise SystemExit('Gemeinsame Bruchfunktionsforschung liefert nicht den erwarteten Befund.')
    print('Drei Strategien im gemeinsamen Lauf:', combined['status'])
    for child in combined['strategy_runs']:
        print('  ', child['strategy'], '→', child['status'])
    print('Gesamtarbeit:', combined['candidates_executed'], 'Kandidaten,', combined['operations_used'], 'Operationseinheiten')
    if json.loads(subprocess.check_output([str(binary), 'replay', str(combined_bundle)], text=True)) != combined:
        raise SystemExit('Replay des Dreistrategien-Portfolios stimmt nicht überein.')
    ordered_args = [str(binary), 'research', '--strategy', 'portfolio-rational',
                    '--train', str(root/'examples/fraction-training.csv'),
                    '--holdout', str(root/'examples/fraction-holdout.csv'), '--max-degree', '2']
    ordered_bundle = Path(directory) / 'ordered-portfolio.bruce'
    ordered = json.loads(subprocess.check_output(ordered_args + [
        '--planner', 'adaptive-order', '--output', str(ordered_bundle)
    ], text=True))
    if ordered['status'] != combined['status'] or sorted(ordered['hypotheses'],key=lambda h:h['id']) != sorted(combined['hypotheses'],key=lambda h:h['id']):
        raise SystemExit('Datenabhängige Reihenfolge verändert den vollständigen fachlichen Befund.')
    if json.loads(subprocess.check_output([str(binary), 'replay', str(ordered_bundle)], text=True)) != ordered:
        raise SystemExit('Replay der datenabhängigen Planung stimmt nicht überein.')
    print('Datenabhängige Reihenfolge:', ' → '.join(ordered['strategy_order']))
    print('Planungsgrund:', ordered['ordering_reason'], '– zusätzliche Operationseinheiten:', ordered['ordering_operations'])
    for planner in ['portfolio', 'adaptive-order']:
        partial = json.loads(subprocess.check_output(ordered_args + [
            '--planner', planner, '--max-candidates', '1'
        ], text=True))
        if partial['status'] != 'INCOMPLETE':
            raise SystemExit('Ein früher Teiltreffer darf den Gesamtstatus nicht abschließen.')
        print('Kandidatenbudget 1,', planner, '→', len(partial['hypotheses']), 'Treffer, Gesamtstatus', partial['status'])
    source_bundle = Path(directory) / 'experience-source.bruce'
    source_args = [str(binary), 'research', '--strategy', 'portfolio-rational', '--max-degree', '2',
                   '--train', str(root/'examples/experience-source-training.csv'),
                   '--holdout', str(root/'examples/experience-source-holdout.csv')]
    subprocess.check_output(source_args + ['--output', str(source_bundle)], text=True)
    experience_path = Path(directory) / 'experience.snapshot'
    experience = json.loads(subprocess.check_output([
        str(binary), 'experience', 'add', '--from-run', str(source_bundle), '--output', str(experience_path)
    ], text=True))
    target_args = [str(binary), 'research', '--strategy', 'portfolio-rational', '--max-degree', '2',
                   '--train', str(root/'examples/experience-target-training.csv'),
                   '--holdout', str(root/'examples/experience-target-holdout.csv')]
    baseline = json.loads(subprocess.check_output(target_args + ['--planner', 'adaptive-order'], text=True))
    learned_path = Path(directory) / 'experience-target.bruce'
    learned = json.loads(subprocess.check_output(target_args + [
        '--planner', 'experience-order', '--experience', str(experience_path), '--output', str(learned_path)
    ], text=True))
    if baseline['status'] != learned['status'] or baseline['hypotheses'] != learned['hypotheses']:
        raise SystemExit('Erfahrungsplanung verändert den vollständigen fachlichen Befund.')
    print('Erfahrung aus geprüftem früherem Lauf: erste Strategie', baseline['strategy_order'][0], '→', learned['strategy_order'][0])
    print('Passende Erfahrungen:', learned['experience_records_used'], 'Stimmen:', learned['experience_votes'])
    print('Neue Sucharbeit:', learned['operations_used'], '– separate Nachweisprüfung:', learned['experience_audit_operations'])
    revoked_path = Path(directory) / 'experience-revoked.snapshot'
    subprocess.check_output([str(binary), 'experience', 'revoke', '--store', str(experience_path),
        '--id', experience['records'][0]['id'], '--reason', 'Demo: Erfahrung zurueckgezogen', '--output', str(revoked_path)], text=True)
    fallback = json.loads(subprocess.check_output(target_args + [
        '--planner', 'experience-order', '--experience', str(revoked_path)
    ], text=True))
    if fallback['experience_records_used'] or fallback['strategy_order'] != baseline['strategy_order']:
        raise SystemExit('Widerruf entfernt den Planungseinfluss nicht.')
    print('Nach Widerruf: ursprüngliche Heuristik, erste Strategie', fallback['strategy_order'][0])
    source_bundle.unlink(); experience_path.unlink(); revoked_path.unlink()
    if json.loads(subprocess.check_output([str(binary), 'replay', str(learned_path)], text=True)) != learned:
        raise SystemExit('Replay benötigt ursprüngliche Experience-Dateien.')
    print('Historischer Replay bleibt mit eingebettetem damaligem Erfahrungsstand identisch.')
    quadratic_bundle = Path(directory) / 'quadratic-fraction.bruce'
    quadratic = json.loads(subprocess.check_output([
        str(binary), 'research', '--strategy', 'rational-quadratic',
        '--train', str(root/'examples/quadratic-fraction-training.csv'),
        '--holdout', str(root/'examples/quadratic-fraction-holdout.csv'),
        '--output', str(quadratic_bundle)
    ], text=True))
    if quadratic['status'] != 'VERIFIED_ON_SAMPLES' or quadratic['hypotheses'][0]['denominator'] != ['1','0','1']:
        raise SystemExit('Quadratische Bruchfunktion besteht die unabhängigen Prüfdaten nicht.')
    if json.loads(subprocess.check_output([str(binary), 'replay', str(quadratic_bundle)], text=True)) != quadratic:
        raise SystemExit('Replay der quadratischen Bruchfunktion stimmt nicht überein.')
    print('Quadratischer Nenner:', quadratic['hypotheses'][0]['formula'], '→', quadratic['status'])
    print('Tatsächlich geprüft:', quadratic['candidates_executed'], 'Kandidat,', quadratic['operations_used'], 'Operationseinheiten')
    quadratic_selection = json.loads(subprocess.check_output([
        str(binary), 'compatibility', '--rules', str(root/'examples/quadratic-fraction-selection.rules'),
        '--from-run', str(quadratic_bundle), '--at', '0'
    ], text=True))
    if quadratic_selection['candidates'][0]['value'] != '1':
        raise SystemExit('Z-Projektion der quadratischen Bruchfunktion weicht ab.')
    print('Quadratische Bruchfunktion bei x=0 →', quadratic_selection['candidates'][0]['value'])
    quadratic_pole = json.loads(subprocess.check_output([
        str(binary), 'research', '--strategy', 'rational-quadratic',
        '--train', str(root/'examples/quadratic-pole-training.csv'),
        '--holdout', str(root/'examples/quadratic-pole-holdout.csv')
    ], text=True))
    if quadratic_pole['status'] != 'REFUTED':
        raise SystemExit('Quadratische Nennernullstelle wurde nicht erkannt.')
    print('Erwartete endliche Ausgabe für 1/(x²−1) an x=1:', quadratic_pole['status'])
    four_args = [str(binary), 'research', '--strategy', 'portfolio-quadratic', '--max-degree', '2',
                 '--train', str(root/'examples/quadratic-fraction-training.csv'),
                 '--holdout', str(root/'examples/quadratic-fraction-holdout.csv')]
    four = json.loads(subprocess.check_output(four_args + ['--planner', 'adaptive-order'], text=True))
    if four['status'] != 'VERIFIED_ON_SAMPLES' or len(four['strategy_runs']) != 4:
        raise SystemExit('Viererportfolio liefert nicht den erwarteten vollständigen Befund.')
    print('Vier Familien im gemeinsamen Lauf:', four['status'])
    print('Reihenfolge:', ' → '.join(four['strategy_order']))
    four_source = Path(directory) / 'four-source.bruce'
    subprocess.check_output([
        str(binary), 'research', '--strategy', 'portfolio-quadratic', '--max-degree', '2',
        '--train', str(root/'examples/quadratic-experience-source-training.csv'),
        '--holdout', str(root/'examples/quadratic-experience-source-holdout.csv'),
        '--output', str(four_source)
    ], text=True)
    four_store = Path(directory) / 'four.experience'
    four_experience = json.loads(subprocess.check_output([
        str(binary), 'experience', 'add', '--from-run', str(four_source), '--output', str(four_store)
    ], text=True))
    four_target_args = [str(binary), 'research', '--strategy', 'portfolio-quadratic', '--max-degree', '2',
        '--train', str(root/'examples/quadratic-experience-target-training.csv'),
        '--holdout', str(root/'examples/quadratic-experience-target-holdout.csv')]
    four_baseline = json.loads(subprocess.check_output(four_target_args + ['--planner', 'adaptive-order'], text=True))
    four_bundle = Path(directory) / 'four-learned.bruce'
    four_learned = json.loads(subprocess.check_output(four_target_args + [
        '--planner', 'experience-order', '--experience', str(four_store), '--output', str(four_bundle)
    ], text=True))
    if four_learned['hypotheses'] != four_baseline['hypotheses'] or four_learned['status'] != four_baseline['status']:
        raise SystemExit('Vierer-Erfahrung verändert den vollständigen fachlichen Befund.')
    if four_learned['experience_votes'] != [0,0,0,1] or four_learned['strategy_order'][0] != 'rational-quadratic':
        raise SystemExit('Erfahrung priorisiert die neue Familie nicht wie erwartet.')
    print('Vierer-Erfahrung: erste Strategie', four_baseline['strategy_order'][0], '→', four_learned['strategy_order'][0])
    print('Stimmen:', four_learned['experience_votes'], '– neue Sucharbeit:', four_learned['operations_used'],
          '– separate Nachweisprüfung:', four_learned['experience_audit_operations'])
    four_revoked = Path(directory) / 'four-revoked.experience'
    subprocess.check_output([
        str(binary), 'experience', 'revoke', '--store', str(four_store),
        '--id', four_experience['records'][0]['id'], '--reason', 'Demo: Vierer-Erfahrung zurueckgezogen',
        '--output', str(four_revoked)
    ], text=True)
    four_fallback = json.loads(subprocess.check_output(four_target_args + [
        '--planner', 'experience-order', '--experience', str(four_revoked)
    ], text=True))
    if four_fallback['experience_records_used'] or four_fallback['strategy_order'] != four_baseline['strategy_order']:
        raise SystemExit('Widerruf der Vierer-Erfahrung lässt noch Planungseinfluss zurück.')
    four_source.unlink(); four_store.unlink(); four_revoked.unlink()
    if json.loads(subprocess.check_output([str(binary), 'replay', str(four_bundle)], text=True)) != four_learned:
        raise SystemExit('V7-Replay benötigt frühere Quell- oder Storedateien.')
    print('Vierer-Erfahrung widerrufen; historischer Replay mit damaligem Speicherstand bleibt identisch.')
    expression_bundle = Path(directory) / 'expression.bruce'
    expression = json.loads(subprocess.check_output([
        str(binary), 'research', '--strategy', 'expression', '--min', '1', '--max', '1', '--max-nodes', '7',
        '--train', str(root/'examples/expression-training.csv'),
        '--holdout', str(root/'examples/expression-holdout.csv'), '--output', str(expression_bundle)
    ], text=True))
    confirmed = [h for h in expression['hypotheses'] if h['status'] == 'VERIFIED_ON_SAMPLES']
    if expression['status'] != 'AMBIGUOUS' or len(confirmed) != 3 or not any(h['formula'] == '((x*x)/(x+1))' for h in confirmed):
        raise SystemExit('Ausdruckssynthese liefert nicht den erwarteten begrenzten Befund.')
    print('Ausdruckssynthese:', expression['candidates_executed'], 'Bäume,', expression['operations_used'], 'Operationseinheiten')
    for h in expression['hypotheses']:
        print('  ', h['formula'], '→', h['status'], '– Bedingungen:', ', '.join(h['domain_conditions']))
    print('Gesamtstatus:', expression['status'], '– algebraisch verwandte Schreibweisen bleiben getrennt.')
    if json.loads(subprocess.check_output([str(binary), 'replay', str(expression_bundle)], text=True)) != expression:
        raise SystemExit('Replay der Ausdruckssynthese stimmt nicht überein.')
    expression_selection = Path(directory) / 'expression-selection.bruce'
    selected_expressions = json.loads(subprocess.check_output([
        str(binary), 'compatibility', '--rules', str(root/'examples/quadratic-fraction-selection.rules'),
        '--from-run', str(expression_bundle), '--at', '0', '--output', str(expression_selection)
    ], text=True))
    if len(selected_expressions['candidates']) != 3 or any(c['value'] != '0' for c in selected_expressions['candidates']):
        raise SystemExit('Ausdrucksprojektion stimmt nicht mit dem geprüften Sample überein.')
    expression_bundle.unlink()
    if json.loads(subprocess.check_output([str(binary), 'replay', str(expression_selection)], text=True)) != selected_expressions:
        raise SystemExit('Auswahl-Replay benötigt die ursprüngliche Ausdrucksdatei.')
    print('Drei passende Ausdrücke bei x=0 → 0; Auswahl-Replay funktioniert ohne Ursprungsdatei.')
    normalized_bundle = Path(directory) / 'expression-normalized.bruce'
    normalized = json.loads(subprocess.check_output([
        str(binary), 'research', '--strategy', 'expression', '--equivalence', 'rational',
        '--min', '1', '--max', '1', '--max-nodes', '7',
        '--train', str(root/'examples/expression-training.csv'),
        '--holdout', str(root/'examples/expression-holdout.csv'), '--output', str(normalized_bundle)
    ], text=True))
    if normalized['status'] != 'VERIFIED_ON_SAMPLES' or len(normalized['hypotheses']) != 2:
        raise SystemExit('Algebraische Zusammenführung liefert nicht die erwarteten zwei Gruppen.')
    good, bad = normalized['hypotheses']
    if len(good['expression_origins']) != 3 or len(bad['expression_origins']) != 2 or bad['status'] != 'REFUTED':
        raise SystemExit('Die Zusammenführung verliert Herkunft oder Definitionslücken.')
    print('Algebraische Zusammenführung:', expression['status'], '→', normalized['status'])
    print('  Drei Schreibweisen von x²/(x+1), Bedingung x+1 != 0; alle Ursprungsbäume erhalten.')
    print('  Zwei weitere Schreibweisen mit zusätzlicher Lücke bei x=0 bleiben getrennt und widerlegt.')
    print('  Aufwand:', normalized['operations_used'], 'Operationseinheiten, davon',
          normalized['normalization_operations'], 'für Normalisierung und deren Samplevergleich.')
    if json.loads(subprocess.check_output([str(binary), 'replay', str(normalized_bundle)], text=True)) != normalized:
        raise SystemExit('Replay der algebraischen Zusammenführung stimmt nicht überein.')
    normalized_selection = Path(directory) / 'normalized-selection.bruce'
    selected_group = json.loads(subprocess.check_output([
        str(binary), 'compatibility', '--rules', str(root/'examples/quadratic-fraction-selection.rules'),
        '--from-run', str(normalized_bundle), '--at', '0', '--output', str(normalized_selection)
    ], text=True))
    if selected_group['status'] != 'SELECTED' or len(selected_group['candidates']) != 1 or selected_group['candidates'][0]['value'] != '0':
        raise SystemExit('Die normalisierte Ausdrucksgruppe wird nicht eindeutig auf 0 projiziert.')
    normalized_bundle.unlink()
    if json.loads(subprocess.check_output([str(binary), 'replay', str(normalized_selection)], text=True)) != selected_group:
        raise SystemExit('Normalisierter Auswahl-Replay benötigt die ursprüngliche Forschungsdatei.')
    print('Eine geprüfte Ausdrucksgruppe bei x=0 → 0; Auswahl und Replay erfolgreich.')
    five_bundle = Path(directory) / 'five-families.bruce'
    five = json.loads(subprocess.check_output([
        str(binary), 'research', '--strategy', 'portfolio-expression', '--min', '1', '--max', '1',
        '--max-power', '2', '--max-degree', '2', '--max-nodes', '7',
        '--train', str(root/'examples/expression-training.csv'),
        '--holdout', str(root/'examples/expression-holdout.csv'), '--output', str(five_bundle)
    ], text=True))
    if five['status'] != 'VERIFIED_ON_SAMPLES' or len(five['strategy_runs']) != 5:
        raise SystemExit('Fünferportfolio findet die zusammengesetzte Formel nicht vollständig.')
    print('Fünf Familien gemeinsam:', five['status'])
    for child in five['strategy_runs']:
        print('  ', child['strategy'], '→', child['status'])
    print('  Aufwand:', five['operations_used'], 'Operationseinheiten,', five['candidates_executed'], 'Kandidaten.')
    if json.loads(subprocess.check_output([str(binary), 'replay', str(five_bundle)], text=True)) != five:
        raise SystemExit('Fünferportfolio-Replay stimmt nicht überein.')
    five_selection = Path(directory) / 'five-selection.bruce'
    selected_five = json.loads(subprocess.check_output([
        str(binary), 'compatibility', '--rules', str(root/'examples/quadratic-fraction-selection.rules'),
        '--from-run', str(five_bundle), '--at', '0', '--output', str(five_selection)
    ], text=True))
    if selected_five['status'] != 'SELECTED' or selected_five['candidates'][0]['value'] != '0':
        raise SystemExit('Fünferportfolio-Auswahl liefert nicht den erwarteten Wert 0.')
    five_bundle.unlink()
    if json.loads(subprocess.check_output([str(binary), 'replay', str(five_selection)], text=True)) != selected_five:
        raise SystemExit('Fünferportfolio-Auswahl benötigt die ursprüngliche Forschungsdatei.')
    five_shared = json.loads(subprocess.check_output([
        str(binary), 'research', '--strategy', 'portfolio-expression', '--min', '0', '--max', '1',
        '--max-power', '2', '--max-degree', '2', '--max-nodes', '5',
        '--train', str(root/'examples/fraction-training.csv'),
        '--holdout', str(root/'examples/fraction-holdout.csv')
    ], text=True))
    shared_good = [h for h in five_shared['hypotheses'] if h['status'] == 'VERIFIED_ON_SAMPLES']
    if five_shared['status'] != 'VERIFIED_ON_SAMPLES' or len(shared_good) != 1 or {
        o['strategy'] for o in shared_good[0]['origins']
    } != {'rational-linear', 'expression'}:
        raise SystemExit('Familienübergreifende Zusammenführung verliert einen Ursprung.')
    print('1/(x+1): rationale Familie und Ausdruckssuche bilden eine Gruppe; beide Herkünfte bleiben erhalten.')
    five_source = Path(directory) / 'five-source.bruce'
    five_source_args = [str(binary), 'research', '--strategy', 'portfolio-expression', '--min', '1', '--max', '1',
        '--max-power', '2', '--max-degree', '2', '--max-nodes', '7',
        '--train', str(root/'examples/expression-training.csv'),
        '--holdout', str(root/'examples/expression-holdout.csv')]
    subprocess.check_output(five_source_args + ['--planner', 'adaptive-order', '--output', str(five_source)], text=True)
    five_store = Path(directory) / 'five.experience'
    five_record = json.loads(subprocess.check_output([
        str(binary), 'experience', 'add', '--from-run', str(five_source), '--output', str(five_store)
    ], text=True))
    five_target_args = [str(binary), 'research', '--strategy', 'portfolio-expression', '--min', '1', '--max', '1',
        '--max-power', '2', '--max-degree', '2', '--max-nodes', '7',
        '--train', str(root/'examples/expression-experience-target-training.csv'),
        '--holdout', str(root/'examples/expression-experience-target-holdout.csv')]
    five_baseline = json.loads(subprocess.check_output(five_target_args + ['--planner', 'adaptive-order'], text=True))
    five_learned_bundle = Path(directory) / 'five-learned.bruce'
    five_learned = json.loads(subprocess.check_output(five_target_args + [
        '--planner', 'experience-order', '--experience', str(five_store), '--output', str(five_learned_bundle)
    ], text=True))
    if five_learned['hypotheses'] != five_baseline['hypotheses'] or five_learned['status'] != five_baseline['status']:
        raise SystemExit('Fünfer-Erfahrung verändert den vollständigen fachlichen Befund.')
    if five_learned['experience_votes'] != [0,0,0,0,1] or five_learned['strategy_order'][0] != 'expression':
        raise SystemExit('Fünfer-Erfahrung priorisiert die Ausdruckssuche nicht.')
    print('Fünfer-Erfahrung: erste Strategie', five_baseline['strategy_order'][0], '→', five_learned['strategy_order'][0])
    print('  Stimmen:', five_learned['experience_votes'], '– alle fünf Familien vollständig ausgeführt.')
    print('  Neue Sucharbeit:', five_learned['operations_used'], '– separate Nachweisprüfung:', five_learned['experience_audit_operations'])
    five_revoked = Path(directory) / 'five-revoked.experience'
    subprocess.check_output([
        str(binary), 'experience', 'revoke', '--store', str(five_store), '--id', five_record['records'][0]['id'],
        '--reason', 'Demo: Fuenfer-Erfahrung zurueckgezogen', '--output', str(five_revoked)
    ], text=True)
    five_fallback = json.loads(subprocess.check_output(five_target_args + [
        '--planner', 'experience-order', '--experience', str(five_revoked)
    ], text=True))
    if five_fallback['experience_records_used'] or five_fallback['strategy_order'] != five_baseline['strategy_order']:
        raise SystemExit('Widerruf der Fünfer-Erfahrung lässt Planungseinfluss zurück.')
    five_source.unlink(); five_store.unlink(); five_revoked.unlink()
    if json.loads(subprocess.check_output([str(binary), 'replay', str(five_learned_bundle)], text=True)) != five_learned:
        raise SystemExit('V10-Replay benötigt frühere Quell- oder Speicherdateien.')
    print('Fünfer-Erfahrung widerrufen; V10-Replay erhält den damaligen Speicherstand ohne Ursprungsdateien.')
    indexed_bundle = Path(directory) / 'indexed-expression.bruce'
    indexed_args = [str(binary), 'research', '--strategy', 'expression', '--equivalence', 'rational',
        '--min', '1', '--max', '1', '--max-nodes', '7',
        '--train', str(root/'examples/expression-training.csv'), '--holdout', str(root/'examples/expression-holdout.csv')]
    indexed = json.loads(subprocess.check_output(indexed_args + [
        '--planner', 'indexed-grammar', '--output', str(indexed_bundle)
    ], text=True))
    if indexed['hypotheses'] != normalized['hypotheses'] or indexed['status'] != normalized['status']:
        raise SystemExit('Grammatikindex verändert den fachlichen Befund.')
    if indexed['planning_operations'] >= normalized['planning_operations'] or indexed['candidates_executed'] != normalized['candidates_executed']:
        raise SystemExit('Grammatikindex spart im Referenzlauf keine Erzeugungsarbeit.')
    print('Grammatikindex:', indexed['index_entries'], 'Bereichseinträge, unverändert', indexed['candidates_executed'], 'Kandidaten.')
    print('  Planungsarbeit:', normalized['planning_operations'], '→', indexed['planning_operations'])
    print('  Gesamte logische Arbeit:', normalized['operations_used'], '→', indexed['operations_used'])
    budget_args = ['--max-operations', str(indexed['operations_used'])]
    old_limited = json.loads(subprocess.check_output(indexed_args + ['--planner', 'grammar'] + budget_args, text=True))
    new_limited = json.loads(subprocess.check_output(indexed_args + ['--planner', 'indexed-grammar'] + budget_args, text=True))
    if old_limited['status'] != 'INCOMPLETE' or new_limited['status'] != 'VERIFIED_ON_SAMPLES':
        raise SystemExit('Das gemeinsame Operationsbudget zeigt nicht den erwarteten Unterschied.')
    print('  Bei gleichem Operationsbudget:', old_limited['status'], '→', new_limited['status'])
    if json.loads(subprocess.check_output([str(binary), 'replay', str(indexed_bundle)], text=True)) != indexed:
        raise SystemExit('Replay der indizierten Ausdruckssuche stimmt nicht überein.')
    indexed_selection = Path(directory) / 'indexed-selection.bruce'
    selected_indexed = json.loads(subprocess.check_output([
        str(binary), 'compatibility', '--rules', str(root/'examples/quadratic-fraction-selection.rules'),
        '--from-run', str(indexed_bundle), '--at', '0', '--output', str(indexed_selection)
    ], text=True))
    if selected_indexed['status'] != 'SELECTED' or selected_indexed['candidates'][0]['value'] != '0':
        raise SystemExit('Indizierte Ausdrucksauswahl liefert nicht 0.')
    indexed_bundle.unlink()
    if json.loads(subprocess.check_output([str(binary), 'replay', str(indexed_selection)], text=True)) != selected_indexed:
        raise SystemExit('Indizierter Auswahl-Replay benötigt den Ursprungsrun.')
    indexed_five_args = five_source_args + ['--expression-planner', 'indexed-grammar']
    indexed_five = json.loads(subprocess.check_output(indexed_five_args, text=True))
    if indexed_five['hypotheses'] != five['hypotheses'] or indexed_five['status'] != five['status']:
        raise SystemExit('Portfolio-Grammatikindex verändert den vollständigen Befund.')
    if five['operations_used'] - indexed_five['operations_used'] != 12221:
        raise SystemExit('Portfolio-Grammatikindex zeigt nicht die erwartete Ersparnis.')
    print('Index im Fünferportfolio:', five['operations_used'], '→', indexed_five['operations_used'], 'logische Operationseinheiten.')
    indexed_five_source = Path(directory) / 'indexed-five-source.bruce'
    subprocess.check_output(indexed_five_args + ['--planner', 'adaptive-order', '--output', str(indexed_five_source)], text=True)
    indexed_five_store = Path(directory) / 'indexed-five.experience'
    indexed_record = json.loads(subprocess.check_output([
        str(binary), 'experience', 'add', '--from-run', str(indexed_five_source), '--output', str(indexed_five_store)
    ], text=True))
    indexed_five_target = five_target_args + ['--expression-planner', 'indexed-grammar']
    indexed_five_learned_path = Path(directory) / 'indexed-five-learned.bruce'
    indexed_five_learned = json.loads(subprocess.check_output(indexed_five_target + [
        '--planner', 'experience-order', '--experience', str(indexed_five_store), '--output', str(indexed_five_learned_path)
    ], text=True))
    if indexed_five_learned['hypotheses'] != five_learned['hypotheses'] or indexed_five_learned['status'] != five_learned['status']:
        raise SystemExit('Indizierte Erfahrungsplanung verändert den vollständigen Befund.')
    if indexed_five_learned['experience_votes'] != [0,0,0,0,1] or indexed_five_learned['strategy_order'][0] != 'expression':
        raise SystemExit('Indizierte Erfahrung priorisiert Ausdruckssuche nicht.')
    mismatch = json.loads(subprocess.check_output(five_target_args + [
        '--planner', 'experience-order', '--experience', str(indexed_five_store)
    ], text=True))
    if mismatch['experience_records_used']:
        raise SystemExit('Erfahrung beeinflusst den falschen Ausdrucksplaner.')
    print('  Mit Erfahrung: Ausdruckssuche zuerst, Stimmen', indexed_five_learned['experience_votes'])
    print('  Sucharbeit:', indexed_five_learned['operations_used'], '– separate Nachweisprüfung:', indexed_five_learned['experience_audit_operations'])
    indexed_revoked = Path(directory) / 'indexed-five-revoked.experience'
    subprocess.check_output([
        str(binary), 'experience', 'revoke', '--store', str(indexed_five_store), '--id', indexed_record['records'][0]['id'],
        '--reason', 'Demo: Indexquelle zurueckgezogen', '--output', str(indexed_revoked)
    ], text=True)
    indexed_fallback = json.loads(subprocess.check_output(indexed_five_target + [
        '--planner', 'experience-order', '--experience', str(indexed_revoked)
    ], text=True))
    if indexed_fallback['experience_records_used'] or indexed_fallback['strategy_order'] != five_baseline['strategy_order']:
        raise SystemExit('Indizierte Erfahrung beeinflusst nach Widerruf noch die Reihenfolge.')
    indexed_five_source.unlink(); indexed_five_store.unlink(); indexed_revoked.unlink()
    if json.loads(subprocess.check_output([str(binary), 'replay', str(indexed_five_learned_path)], text=True)) != indexed_five_learned:
        raise SystemExit('V13-Replay benötigt externe Erfahrungsdateien.')
    indexed_five_selection = Path(directory) / 'indexed-five-selection.bruce'
    indexed_five_selected = json.loads(subprocess.check_output([
        str(binary), 'compatibility', '--rules', str(root/'examples/quadratic-fraction-selection.rules'),
        '--from-run', str(indexed_five_learned_path), '--at', '0', '--output', str(indexed_five_selection)
    ], text=True))
    if indexed_five_selected['status'] != 'SELECTED' or indexed_five_selected['candidates'][0]['value'] != '0':
        raise SystemExit('Indizierte Portfolio-Projektion liefert nicht 0.')
    indexed_five_learned_path.unlink()
    if json.loads(subprocess.check_output([str(binary), 'replay', str(indexed_five_selection)], text=True)) != indexed_five_selected:
        raise SystemExit('Indizierte Portfolio-Auswahl benötigt ihre Ursprungsdatei.')
    print('  Widerruf, V13-Replay und Z-Auswahl mit Wert 0 geprüft; ursprüngliche Quellen entfernt.')
    operator_args = [str(binary), 'research', '--strategy', 'expression', '--planner', 'indexed-grammar',
        '--equivalence', 'rational', '--min', '1', '--max', '1', '--max-nodes', '5',
        '--train', str(root/'examples/selected-operators-training.csv'),
        '--holdout', str(root/'examples/selected-operators-holdout.csv')]
    all_operators = json.loads(subprocess.check_output(operator_args, text=True))
    operator_bundle = Path(directory) / 'selected-operators.bruce'
    selected_operators = json.loads(subprocess.check_output(operator_args + [
        '--operators', '+*', '--output', str(operator_bundle)
    ], text=True))
    if selected_operators['status'] != 'VERIFIED_ON_SAMPLES' or selected_operators['grammar_expressions'] != 32:
        raise SystemExit('Ausgewählte Rechenarten finden x²+x nicht vollständig.')
    good_forms = lambda run: [h['normal_form'] for h in run['hypotheses'] if h['status'] == 'VERIFIED_ON_SAMPLES']
    if good_forms(all_operators) != good_forms(selected_operators):
        raise SystemExit('Der Operatorvergleich findet nicht dieselbe geprüfte Beispielnormalform.')
    if selected_operators['operations_used'] >= all_operators['operations_used']:
        raise SystemExit('Der kleinere Beispielraum spart keine logische Sucharbeit.')
    only_add = json.loads(subprocess.check_output(operator_args + ['--operators', '+'], text=True))
    if only_add['status'] != 'NO_CANDIDATE':
        raise SystemExit('Reine Addition erklärt die quadratischen Beispielpaare unerwartet.')
    print('Ausgewählte Rechenarten für x²+x:', all_operators['grammar_expressions'], '→', selected_operators['grammar_expressions'], 'Ausdrücke.')
    print('  Logische Arbeit:', all_operators['operations_used'], '→', selected_operators['operations_used'])
    print('  Nur Addition:', only_add['status'], '– Aussage gilt ausschließlich für die deklarierte Grammatik.')
    if json.loads(subprocess.check_output([str(binary), 'replay', str(operator_bundle)], text=True)) != selected_operators:
        raise SystemExit('Replay verliert die Operatorauswahl.')
    operator_selection = Path(directory) / 'selected-operators-selection.bruce'
    projected_operators = json.loads(subprocess.check_output([
        str(binary), 'compatibility', '--rules', str(root/'examples/quadratic-fraction-selection.rules'),
        '--from-run', str(operator_bundle), '--at', '0', '--output', str(operator_selection)
    ], text=True))
    if projected_operators['status'] != 'SELECTED' or projected_operators['candidates'][0]['value'] != '0':
        raise SystemExit('Operatorauswahl projiziert x=0 nicht korrekt.')
    operator_bundle.unlink()
    if json.loads(subprocess.check_output([str(binary), 'replay', str(operator_selection)], text=True)) != projected_operators:
        raise SystemExit('Operatorauswahl-Replay benötigt die ursprüngliche Forschungsdatei.')
    selected_five_args = [str(binary), 'research', '--strategy', 'portfolio-expression',
        '--expression-planner', 'indexed-grammar', '--min', '1', '--max', '1',
        '--max-power', '2', '--max-degree', '2', '--max-nodes', '5',
        '--train', str(root/'examples/selected-operators-training.csv'),
        '--holdout', str(root/'examples/selected-operators-holdout.csv')]
    all_five_operators = json.loads(subprocess.check_output(selected_five_args, text=True))
    selected_five_source = Path(directory) / 'selected-five-source.bruce'
    selected_five_operators = json.loads(subprocess.check_output(selected_five_args + [
        '--operators', '+*', '--output', str(selected_five_source)
    ], text=True))
    if selected_five_operators['status'] != 'VERIFIED_ON_SAMPLES' or len(selected_five_operators['strategy_runs']) != 5:
        raise SystemExit('Portfolio mit gewählten Operatoren bleibt unerwartet unvollständig.')
    if selected_five_operators['strategy_runs'][:4] != all_five_operators['strategy_runs'][:4]:
        raise SystemExit('Operatorauswahl verändert eine andere Familie als Ausdruckssuche.')
    if good_forms(selected_five_operators) != good_forms(all_five_operators):
        raise SystemExit('Portfoliovergleich liefert nicht dieselbe geprüfte Beispielnormalform.')
    print('Operatorauswahl im Fünferportfolio:', all_five_operators['operations_used'], '→', selected_five_operators['operations_used'], 'logische Einheiten.')
    print('  Die anderen vier Teilruns bleiben identisch; Ausdrucksraum:', selected_five_operators['strategy_runs'][4]['grammar_expressions'])
    selected_five_store = Path(directory) / 'selected-five.experience'
    selected_five_record = json.loads(subprocess.check_output([
        str(binary), 'experience', 'add', '--from-run', str(selected_five_source), '--output', str(selected_five_store)
    ], text=True))
    selected_five_target = [str(binary), 'research', '--strategy', 'portfolio-expression',
        '--expression-planner', 'indexed-grammar', '--operators', '+*', '--min', '1', '--max', '1',
        '--max-power', '2', '--max-degree', '2', '--max-nodes', '5',
        '--train', str(root/'examples/selected-operators-target-training.csv'),
        '--holdout', str(root/'examples/selected-operators-target-holdout.csv')]
    selected_five_adaptive = json.loads(subprocess.check_output(selected_five_target + ['--planner', 'adaptive-order'], text=True))
    selected_five_learned_path = Path(directory) / 'selected-five-learned.bruce'
    selected_five_learned = json.loads(subprocess.check_output(selected_five_target + [
        '--planner', 'experience-order', '--experience', str(selected_five_store), '--output', str(selected_five_learned_path)
    ], text=True))
    if selected_five_learned['hypotheses'] != selected_five_adaptive['hypotheses'] or selected_five_learned['status'] != selected_five_adaptive['status']:
        raise SystemExit('Erfahrungsreihenfolge verändert den vollständigen Befund der gewählten Grammatik.')
    if selected_five_learned['experience_records_used'] != 1 or selected_five_learned['strategy_order'][0] != 'expression':
        raise SystemExit('Passende Operatorerfahrung wird nicht verwendet.')
    print('  Passende Erfahrung:', selected_five_learned['experience_votes'], '– Ausdruckssuche zuerst.')
    print('  Neue Sucharbeit:', selected_five_learned['operations_used'], '– separate Quellenprüfung:', selected_five_learned['experience_audit_operations'])
    revoked_selected_five = Path(directory) / 'selected-five-revoked.experience'
    subprocess.check_output([str(binary), 'experience', 'revoke', '--store', str(selected_five_store),
        '--id', selected_five_record['records'][0]['id'], '--reason', 'Demo: Operatorquelle widerrufen', '--output', str(revoked_selected_five)], text=True)
    revoked_target = json.loads(subprocess.check_output(selected_five_target + [
        '--planner', 'experience-order', '--experience', str(revoked_selected_five)
    ], text=True))
    if revoked_target['experience_records_used'] or revoked_target['strategy_order'] != selected_five_adaptive['strategy_order']:
        raise SystemExit('Widerruf entfernt Operatorerfahrung nicht.')
    selected_five_source.unlink(); selected_five_store.unlink(); revoked_selected_five.unlink()
    if json.loads(subprocess.check_output([str(binary), 'replay', str(selected_five_learned_path)], text=True)) != selected_five_learned:
        raise SystemExit('V16-Replay benötigt alte Quell- oder Speicherdateien.')
    selected_five_projection = Path(directory) / 'selected-five-projection.bruce'
    selected_five_projected = json.loads(subprocess.check_output([str(binary), 'compatibility',
        '--rules', str(root/'examples/quadratic-fraction-selection.rules'), '--from-run', str(selected_five_learned_path),
        '--at', '0', '--output', str(selected_five_projection)], text=True))
    if selected_five_projected['status'] != 'SELECTED' or selected_five_projected['candidates'][0]['value'] != '0':
        raise SystemExit('Operatorportfolio liefert nicht die erwartete Auswahl 0.')
    selected_five_learned_path.unlink()
    if json.loads(subprocess.check_output([str(binary), 'replay', str(selected_five_projection)], text=True)) != selected_five_projected:
        raise SystemExit('Operatorportfolio-Auswahl benötigt den ursprünglichen Forschungsrun.')
    print('  Widerruf, V16-Replay und Auswahl-Replay ohne ursprüngliche Dateien geprüft.')
    expression_power_args = [str(binary), 'research', '--strategy', 'expression', '--planner', 'indexed-grammar',
        '--equivalence', 'rational', '--min', '1', '--max', '3',
        '--train', str(root/'examples/expression-power-training.csv'),
        '--holdout', str(root/'examples/expression-power-holdout.csv')]
    short_products = json.loads(subprocess.check_output(expression_power_args + ['--operators', '+*', '--max-nodes', '5'], text=True))
    long_products = json.loads(subprocess.check_output(expression_power_args + ['--operators', '+*', '--max-nodes', '7'], text=True))
    expression_power_bundle = Path(directory) / 'expression-power.bruce'
    expression_power = json.loads(subprocess.check_output(expression_power_args + [
        '--operators', '+^', '--max-nodes', '5', '--output', str(expression_power_bundle)
    ], text=True))
    if short_products['status'] != 'NO_CANDIDATE' or expression_power['status'] != 'VERIFIED_ON_SAMPLES':
        raise SystemExit('Begrenzte Potenzgrammatik findet x³+x nicht mit fünf Knoten.')
    if good_forms(expression_power) != good_forms(long_products):
        raise SystemExit('Potenz- und Produktbeispiel ergeben nicht dieselbe Normalform.')
    print('Potenzen im Ausdruck: x³+x mit fünf statt sieben Baumknoten.')
    print('  Produktgrammatik mit fünf Knoten:', short_products['status'])
    print('  Kandidaten (Produkte/sieben Knoten → Potenzen/fünf Knoten):', long_products['grammar_expressions'], '→', expression_power['grammar_expressions'])
    print('  Logische Arbeit:', long_products['operations_used'], '→', expression_power['operations_used'])
    if json.loads(subprocess.check_output([str(binary), 'replay', str(expression_power_bundle)], text=True)) != expression_power:
        raise SystemExit('V5-Replay der Potenzausdrücke stimmt nicht überein.')
    expression_power_selection = Path(directory) / 'expression-power-selection.bruce'
    power_projection = json.loads(subprocess.check_output([str(binary), 'compatibility',
        '--rules', str(root/'examples/quadratic-fraction-selection.rules'), '--from-run', str(expression_power_bundle),
        '--at', '0', '--output', str(expression_power_selection)], text=True))
    if power_projection['status'] != 'SELECTED' or power_projection['candidates'][0]['value'] != '0':
        raise SystemExit('Potenzausdruck projiziert x=0 nicht korrekt.')
    expression_power_bundle.unlink()
    if json.loads(subprocess.check_output([str(binary), 'replay', str(expression_power_selection)], text=True)) != power_projection:
        raise SystemExit('Potenzauswahl-Replay benötigt die ursprüngliche Forschungsdatei.')
    power_five_args = [str(binary), 'research', '--strategy', 'portfolio-expression',
        '--expression-planner', 'indexed-grammar', '--min', '1', '--max', '3', '--max-power', '2', '--max-degree', '2']
    power_five_pairs = ['--train', str(root/'examples/expression-power-training.csv'),
        '--holdout', str(root/'examples/expression-power-holdout.csv')]
    power_five_source = Path(directory) / 'power-five-source.bruce'
    power_five = json.loads(subprocess.check_output(power_five_args + power_five_pairs + [
        '--operators', '+^', '--max-nodes', '5', '--output', str(power_five_source)], text=True))
    product_five = json.loads(subprocess.check_output(power_five_args + power_five_pairs + [
        '--operators', '+*', '--max-nodes', '7'], text=True))
    if power_five['status'] != 'VERIFIED_ON_SAMPLES' or good_forms(power_five) != good_forms(product_five):
        raise SystemExit('Potenzportfolio und Produktportfolio liefern verschiedene bestätigte Normalformen.')
    if power_five['strategy_runs'][:4] != product_five['strategy_runs'][:4]:
        raise SystemExit('Potenzoperator verändert andere Familien.')
    print('Potenzen im Fünferportfolio: gleiche Formelgruppe, andere vier Suchwege unverändert.')
    print('  Portfolioarbeit (+*/sieben Knoten → +^/fünf Knoten):', product_five['operations_used'], '→', power_five['operations_used'])
    power_five_store = Path(directory) / 'power-five.experience'
    power_five_record = json.loads(subprocess.check_output([str(binary), 'experience', 'add',
        '--from-run', str(power_five_source), '--output', str(power_five_store)], text=True))
    power_five_target = power_five_args + ['--operators', '+^', '--max-nodes', '5',
        '--train', str(root/'examples/expression-power-target-training.csv'),
        '--holdout', str(root/'examples/expression-power-target-holdout.csv')]
    power_five_adaptive = json.loads(subprocess.check_output(power_five_target + ['--planner', 'adaptive-order'], text=True))
    power_five_learned_path = Path(directory) / 'power-five-learned.bruce'
    power_five_learned = json.loads(subprocess.check_output(power_five_target + ['--planner', 'experience-order',
        '--experience', str(power_five_store), '--output', str(power_five_learned_path)], text=True))
    if power_five_learned['hypotheses'] != power_five_adaptive['hypotheses'] or power_five_learned['status'] != 'VERIFIED_ON_SAMPLES':
        raise SystemExit('Potenz-Erfahrungsplanung verändert den fachlichen Befund.')
    if power_five_learned['experience_votes'] != [0,0,0,0,1] or power_five_learned['strategy_order'][0] != 'expression':
        raise SystemExit('Potenz-Erfahrung ordnet den Ausdrucksweg nicht zuerst.')
    print('  Potenzerfahrung:', power_five_learned['experience_votes'], '– Ausdruckssuche zuerst, alle fünf Familien geprüft.')
    print('  Neue Sucharbeit:', power_five_learned['operations_used'], '– separate Quellenprüfung:', power_five_learned['experience_audit_operations'])
    power_five_revoked = Path(directory) / 'power-five-revoked.experience'
    subprocess.check_output([str(binary), 'experience', 'revoke', '--store', str(power_five_store),
        '--id', power_five_record['records'][0]['id'], '--reason', 'Demo: Potenzquelle widerrufen', '--output', str(power_five_revoked)], text=True)
    power_five_fallback = json.loads(subprocess.check_output(power_five_target + ['--planner', 'experience-order',
        '--experience', str(power_five_revoked)], text=True))
    if power_five_fallback['experience_records_used'] or power_five_fallback['strategy_order'] != power_five_adaptive['strategy_order']:
        raise SystemExit('Widerruf entfernt Potenzerfahrung nicht.')
    power_five_source.unlink(); power_five_store.unlink(); power_five_revoked.unlink()
    if json.loads(subprocess.check_output([str(binary), 'replay', str(power_five_learned_path)], text=True)) != power_five_learned:
        raise SystemExit('V19-Replay benötigt ursprüngliche Erfahrungsdateien.')
    power_five_selection = Path(directory) / 'power-five-selection.bruce'
    power_five_projection = json.loads(subprocess.check_output([str(binary), 'compatibility',
        '--rules', str(root/'examples/quadratic-fraction-selection.rules'), '--from-run', str(power_five_learned_path),
        '--at', '0', '--output', str(power_five_selection)], text=True))
    if power_five_projection['status'] != 'SELECTED' or power_five_projection['candidates'][0]['value'] != '0':
        raise SystemExit('Potenzportfolio liefert nicht die erwartete Auswahl 0.')
    power_five_learned_path.unlink()
    if json.loads(subprocess.check_output([str(binary), 'replay', str(power_five_selection)], text=True)) != power_five_projection:
        raise SystemExit('Potenzportfolio-Auswahl benötigt die Forschungsdatei.')
    print('  Widerruf, V19-Replay und Auswahl-Replay ohne ursprüngliche Dateien geprüft.')
    cached_bundle = Path(directory) / 'cached-expression.bruce'
    cached = json.loads(subprocess.check_output(indexed_args + [
        '--planner', 'cached-grammar', '--output', str(cached_bundle)], text=True))
    if cached['hypotheses'] != indexed['hypotheses'] or cached['candidates_executed'] != indexed['candidates_executed']:
        raise SystemExit('Teilbaumcache verändert Kandidaten oder Befunde.')
    if cached['planning_operations'] >= indexed['planning_operations'] or cached['cache_entries'] > 64:
        raise SystemExit('Teilbaumcache spart im Beispiel keine Planungsarbeit oder überschreitet seine Grenze.')
    print('Teilbaumcache: unverändert', cached['grammar_expressions'], 'Ausdrücke; belegte Einträge:', cached['cache_entries'])
    print('  Treffer:', cached['cache_hits'], '– Fehlzugriffe:', cached['cache_misses'], '– Verdrängungen:', cached['cache_evictions'])
    print('  Planungsarbeit:', indexed['planning_operations'], '→', cached['planning_operations'])
    print('  Gesamte logische Arbeit:', indexed['operations_used'], '→', cached['operations_used'])
    limited_cached = json.loads(subprocess.check_output(indexed_args + ['--planner', 'cached-grammar',
        '--max-operations', str(cached['operations_used'])], text=True))
    limited_indexed = json.loads(subprocess.check_output(indexed_args + ['--planner', 'indexed-grammar',
        '--max-operations', str(cached['operations_used'])], text=True))
    if limited_cached['status'] != 'VERIFIED_ON_SAMPLES' or limited_indexed['status'] != 'INCOMPLETE':
        raise SystemExit('Gemeinsames Vergleichsbudget bestätigt den Cachevorteil nicht.')
    if json.loads(subprocess.check_output([str(binary), 'replay', str(cached_bundle)], text=True)) != cached:
        raise SystemExit('Cache-Replay rekonstruiert den Lauf nicht aus leerem Speicher.')
    cached_selection = Path(directory) / 'cached-selection.bruce'
    cached_projected = json.loads(subprocess.check_output([str(binary), 'compatibility',
        '--rules', str(root/'examples/quadratic-fraction-selection.rules'), '--from-run', str(cached_bundle),
        '--at', '0', '--output', str(cached_selection)], text=True))
    if cached_projected['status'] != 'SELECTED' or cached_projected['candidates'][0]['value'] != '0':
        raise SystemExit('Cache-Auswahl projiziert nicht korrekt.')
    cached_bundle.unlink()
    if json.loads(subprocess.check_output([str(binary), 'replay', str(cached_selection)], text=True)) != cached_projected:
        raise SystemExit('Cache-Auswahl benötigt die ursprüngliche Forschungsdatei.')
    print('  Gleiches Budget: INCOMPLETE → VERIFIED_ON_SAMPLES; V6- und Auswahl-Replay geprüft.')
    cached_five_args = [str(binary), 'research', '--strategy', 'portfolio-expression',
        '--expression-planner', 'cached-grammar', '--min', '1', '--max', '1', '--max-nodes', '7',
        '--max-power', '2', '--max-degree', '2']
    cached_five_source = Path(directory) / 'cached-five-source.bruce'
    cached_five = json.loads(subprocess.check_output(cached_five_args + [
        '--train', str(root/'examples/expression-training.csv'), '--holdout', str(root/'examples/expression-holdout.csv'),
        '--output', str(cached_five_source)], text=True))
    cached_reference_args = [('indexed-grammar' if x == 'cached-grammar' else x) for x in cached_five_args]
    cached_reference = json.loads(subprocess.check_output(cached_reference_args + [
        '--train', str(root/'examples/expression-training.csv'), '--holdout', str(root/'examples/expression-holdout.csv')], text=True))
    if cached_five['hypotheses'] != cached_reference['hypotheses'] or cached_five['strategy_runs'][:4] != cached_reference['strategy_runs'][:4]:
        raise SystemExit('Cacheportfolio verändert Befunde oder andere Familien.')
    if cached_five['operations_used'] >= cached_reference['operations_used']:
        raise SystemExit('Cacheportfolio spart im Beispiel keine logische Arbeit.')
    print('Teilbaumcache im Fünferportfolio:', cached_reference['operations_used'], '→', cached_five['operations_used'], 'logische Einheiten.')
    cached_five_store = Path(directory) / 'cached-five.experience'
    cached_five_report = json.loads(subprocess.check_output([str(binary), 'experience', 'add',
        '--from-run', str(cached_five_source), '--output', str(cached_five_store)], text=True))
    cached_five_target = cached_five_args + ['--train', str(root/'examples/expression-experience-target-training.csv'),
        '--holdout', str(root/'examples/expression-experience-target-holdout.csv')]
    cached_five_baseline = json.loads(subprocess.check_output(cached_five_target + ['--planner', 'adaptive-order'], text=True))
    cached_five_learned_path = Path(directory) / 'cached-five-learned.bruce'
    cached_five_learned = json.loads(subprocess.check_output(cached_five_target + ['--planner', 'experience-order',
        '--experience', str(cached_five_store), '--output', str(cached_five_learned_path)], text=True))
    if cached_five_learned['hypotheses'] != cached_five_baseline['hypotheses'] or cached_five_learned['status'] != 'VERIFIED_ON_SAMPLES':
        raise SystemExit('Cache-Erfahrung verändert den vollständigen Befund.')
    if cached_five_learned['experience_votes'] != [0,0,0,0,1] or cached_five_learned['strategy_order'][0] != 'expression':
        raise SystemExit('Passende Cachequelle beeinflusst die Reihenfolge nicht.')
    print('  Passende Cache-Erfahrung:', cached_five_learned['experience_votes'], '– Ausdruckssuche zuerst.')
    print('  Sucharbeit:', cached_five_learned['operations_used'], '– separate Quellenprüfung:', cached_five_learned['experience_audit_operations'])
    cached_five_revoked = Path(directory) / 'cached-five-revoked.experience'
    subprocess.check_output([str(binary), 'experience', 'revoke', '--store', str(cached_five_store),
        '--id', cached_five_report['records'][0]['id'], '--reason', 'Demo: Cachequelle widerrufen', '--output', str(cached_five_revoked)], text=True)
    cached_five_fallback = json.loads(subprocess.check_output(cached_five_target + ['--planner', 'experience-order',
        '--experience', str(cached_five_revoked)], text=True))
    if cached_five_fallback['experience_records_used'] or cached_five_fallback['strategy_order'] != cached_five_baseline['strategy_order']:
        raise SystemExit('Widerruf entfernt Cache-Erfahrung nicht.')
    cached_five_source.unlink(); cached_five_store.unlink(); cached_five_revoked.unlink()
    if json.loads(subprocess.check_output([str(binary), 'replay', str(cached_five_learned_path)], text=True)) != cached_five_learned:
        raise SystemExit('V22-Replay benötigt frühere Quellen oder Speicherdateien.')
    cached_five_selection = Path(directory) / 'cached-five-selection.bruce'
    cached_five_projected = json.loads(subprocess.check_output([str(binary), 'compatibility',
        '--rules', str(root/'examples/quadratic-fraction-selection.rules'), '--from-run', str(cached_five_learned_path),
        '--at', '0', '--output', str(cached_five_selection)], text=True))
    if cached_five_projected['status'] != 'SELECTED' or cached_five_projected['candidates'][0]['value'] != '0':
        raise SystemExit('Cacheportfolio-Auswahl liefert nicht 0.')
    cached_five_learned_path.unlink()
    if json.loads(subprocess.check_output([str(binary), 'replay', str(cached_five_selection)], text=True)) != cached_five_projected:
        raise SystemExit('Cacheportfolio-Auswahl benötigt die Ursprungsforschung.')
    print('  Widerruf, V22-Replay und Auswahl-Replay ohne ursprüngliche Dateien geprüft.')
    print('Die temporären Replay-Pakete werden nach dieser Demo entfernt.')

with tempfile.TemporaryDirectory(prefix='bruce-domain-demo-') as directory:
    print('\nCore 0.26 — Exakter Vergleich von Polynombrüchen über Q')
    for index, (polynomial, expected) in enumerate([
        ('1,0,1', 'PROVEN_EQUIVALENT'),
        ('-2,0,1', 'PROVEN_EQUIVALENT'),
        ('0,1', 'PROVEN_DIFFERENT'),
        ('-2,0,0,1', 'UNDECIDED'),
    ]):
        output = Path(directory)/f'comparison-{index}.bruce'
        command = [str(binary), 'compare', '--left-numerator', polynomial,
            '--left-denominator', polynomial, '--right-numerator', '1',
            '--right-denominator', '1', '--output', str(output)]
        result = json.loads(subprocess.check_output(command, text=True))
        if result['status'] != expected:
            raise SystemExit('Falscher Definitionsbereichsbefund.')
        repeated = json.loads(subprocess.check_output([str(binary), 'replay', str(output)], text=True))
        if repeated != result:
            raise SystemExit('Definitionsbereichs-Replay weicht ab.')
        print('  Koeffizienten', polynomial, ': p(x)/p(x) gegenüber 1 →', result['status'],
              '– logische Einheiten:', result['operations'])
        witness = result['domain_certificate']['witness']
        if witness is not None:
            print('  Gegenprüfte unterschiedliche Ausschlussstelle:', witness)
    print('  Alle vier Vergleiche einschließlich offener Entscheidung identisch reproduziert.')

with tempfile.TemporaryDirectory(prefix='bruce-q-expression-demo-') as directory:
    print('\nCore 0.27 — Rationale Definitionsbereiche direkt in der Ausdruckssuche')
    output=Path(directory)/'complete.bruce'
    complete=json.loads(subprocess.check_output([str(binary),'research','--strategy','expression',
        '--planner','cached-grammar','--equivalence','rational-q','--min','1','--max','1','--max-nodes','7',
        '--train',str(root/'examples/quadratic-fraction-training.csv'),
        '--holdout',str(root/'examples/quadratic-fraction-holdout.csv'),'--output',str(output)],text=True))
    if complete['status']!='VERIFIED_ON_SAMPLES' or complete['stop_reason']!='Completed':
        raise SystemExit('Vollständige Q-Ausdruckssuche scheitert.')
    if json.loads(subprocess.check_output([str(binary),'replay',str(output)],text=True))!=complete:
        raise SystemExit('Q-Ausdrucksreplay weicht ab.')
    print('  Vollständige Suche:',complete['hypotheses'][0]['formula'],'→',complete['status'])
    print('  Logische Arbeit:',complete['operations_used'],'– davon Gruppen-/Bereichsvergleich:',complete['domain_comparison_operations'])
    for constant in [2,3]:
        results=[]
        for equivalence in ['rational','rational-q']:
            target=Path(directory)/f'{constant}-{equivalence}.bruce'
            result=json.loads(subprocess.check_output([str(binary),'research','--strategy','expression',
                '--planner','cached-grammar','--equivalence',equivalence,'--operators','-^/',
                '--min',str(constant),'--max',str(constant),'--max-nodes','9','--max-operations','10000000',
                '--train',str(root/'examples/q-domain-zero-training.csv'),
                '--holdout',str(root/'examples/q-domain-zero-holdout.csv'),'--output',str(target)],text=True))
            if json.loads(subprocess.check_output([str(binary),'replay',str(target)],text=True))!=result:
                raise SystemExit('Teilbefund der Q-Ausdruckssuche nicht reproduzierbar.')
            results.append(result)
        old,new=results
        def source_list(run):
            return sorted((o['id'],o['formula'],o['status']) for h in run['hypotheses'] for o in h['expression_origins'])
        if source_list(old)!=source_list(new) or any(r['stop_reason']!='ResultBudgetExceeded' for r in results):
            raise SystemExit('Q-Gruppierung verändert Ursprünge oder ignoriert Speichergrenze.')
        if constant==2 and len(old['hypotheses'])-len(new['hypotheses'])!=1:
            raise SystemExit('Nullstellenfreier quadratischer Ausschluss nicht zusammengeführt.')
        if constant==3 and (len(old['hypotheses'])!=len(new['hypotheses']) or not new['domain_unknown']):
            raise SystemExit('Offener kubischer Bereichsvergleich wurde unzulässig vereinigt.')
        print('  Konstante',constant,': Gruppen',len(old['hypotheses']),'→',len(new['hypotheses']),
              '– offene Vergleiche:',new['domain_unknown'])
        print('  Beide Läufe INCOMPLETE an 128 Ursprüngen; zusätzliche Q-Prüfarbeit:',new['domain_comparison_operations'])

with tempfile.TemporaryDirectory(prefix='bruce-q-portfolio-demo-') as directory:
    print('\nCore 0.28 — Q-Gruppen über fünf Familien und passende Erfahrung')
    base=[str(binary),'research','--strategy','portfolio-expression','--expression-planner','cached-grammar',
          '--operators','+/','--min','0','--max','1','--max-power','1','--max-degree','1','--max-nodes','3',
          '--train',str(root/'examples/q-domain-zero-training.csv'),'--holdout',str(root/'examples/q-domain-zero-holdout.csv')]
    old=json.loads(subprocess.check_output(base+['--equivalence','rational'],text=True))
    grouped=Path(directory)/'q-zero.bruce'
    new=json.loads(subprocess.check_output(base+['--equivalence','rational-q','--output',str(grouped)],text=True))
    if old['status']!='AMBIGUOUS' or new['status']!='VERIFIED_ON_SAMPLES' or len(new['hypotheses'])!=1:
        raise SystemExit('Q-Familienvereinigung liefert falschen Befund.')
    if new['stored_source_hypotheses']!=old['stored_source_hypotheses']:
        raise SystemExit('Q-Familienvereinigung verliert Ursprünge.')
    if json.loads(subprocess.check_output([str(binary),'replay',str(grouped)],text=True))!=new:
        raise SystemExit('Q-Portfolio-Replay weicht ab.')
    print('  Vollständige Nullfunktionssuche:',len(old['hypotheses']),'→',len(new['hypotheses']),'Gruppen;',new['stored_source_hypotheses'],'Ursprünge erhalten.')
    print('  Status:',old['status'],'→',new['status'],'– logische Arbeit:',old['operations_used'],'→',new['operations_used'])
    qbase=[str(binary),'research','--strategy','portfolio-expression','--equivalence','rational-q',
           '--expression-planner','cached-grammar','--min','1','--max','1','--max-power','2','--max-degree','2','--max-nodes','7']
    source=Path(directory)/'q-source.bruce'
    subprocess.check_output(qbase+['--planner','adaptive-order','--train',str(root/'examples/expression-training.csv'),
        '--holdout',str(root/'examples/expression-holdout.csv'),'--output',str(source)],text=True)
    store=Path(directory)/'q.experience'
    report=json.loads(subprocess.check_output([str(binary),'experience','add','--from-run',str(source),'--output',str(store)],text=True))
    target=qbase+['--train',str(root/'examples/expression-experience-target-training.csv'),
                  '--holdout',str(root/'examples/expression-experience-target-holdout.csv')]
    baseline=json.loads(subprocess.check_output(target+['--planner','adaptive-order'],text=True))
    learned_path=Path(directory)/'q-learned.bruce'
    learned=json.loads(subprocess.check_output(target+['--planner','experience-order','--experience',str(store),'--output',str(learned_path)],text=True))
    if learned['hypotheses']!=baseline['hypotheses'] or learned['experience_votes']!=[0,0,0,0,1] or learned['strategy_order'][0]!='expression':
        raise SystemExit('Q-Erfahrung verändert Befund oder ordnet nicht passend.')
    revoked=Path(directory)/'q-revoked.experience'
    subprocess.check_output([str(binary),'experience','revoke','--store',str(store),'--id',report['records'][0]['id'],
                            '--reason','Demo Widerruf','--output',str(revoked)],text=True)
    fallback=json.loads(subprocess.check_output(target+['--planner','experience-order','--experience',str(revoked)],text=True))
    if fallback['experience_records_used'] or fallback['strategy_order']!=baseline['strategy_order']:
        raise SystemExit('Q-Widerruf wirkt nicht.')
    selected=Path(directory)/'q-selected.bruce'
    projection=json.loads(subprocess.check_output([str(binary),'compatibility','--rules',str(root/'examples/quadratic-fraction-selection.rules'),
        '--from-run',str(learned_path),'--at','0','--output',str(selected)],text=True))
    if projection['status']!='SELECTED' or projection['candidates'][0]['value']!='0':
        raise SystemExit('Q-Portfolio-Auswahl liefert nicht null.')
    source.unlink();store.unlink();revoked.unlink()
    if json.loads(subprocess.check_output([str(binary),'replay',str(learned_path)],text=True))!=learned:
        raise SystemExit('Q-Erfahrungsreplay benötigt externe Quellen.')
    learned_path.unlink()
    if json.loads(subprocess.check_output([str(binary),'replay',str(selected)],text=True))!=projection:
        raise SystemExit('Q-Auswahlreplay benötigt Forschungsdatei.')
    print('  Erfahrung V8:',learned['experience_votes'],'– Ausdruckssuche zuerst, gleiche vollständige Gruppen.')
    print('  Sucharbeit:',learned['operations_used'],'– zusätzliche Quellenprüfung:',learned['experience_audit_operations'])
    print('  Widerruf, V25-Replay und Auswahl-Replay ohne ursprüngliche Quellen geprüft.')

with tempfile.TemporaryDirectory(prefix='bruce-modular-comparison-demo-') as directory:
    print('\nCore 0.29 — exakter Nullstellenausschluss mit Primzahlzertifikat')
    base=[str(binary),'compare','--left-numerator','-2,0,0,1','--left-denominator','-2,0,0,1',
          '--right-numerator','1','--right-denominator','1']
    old=json.loads(subprocess.check_output(base,text=True))
    target=Path(directory)/'modular-proof.bruce'
    new=json.loads(subprocess.check_output(base+['--domain-policy','modular','--output',str(target)],text=True))
    certificate=new['domain_certificate']
    if old['status']!='UNDECIDED' or new['status']!='PROVEN_EQUIVALENT' or certificate['left_modular_prime']!=7:
        raise SystemExit('Modularer Bereichsbeweis fehlt.')
    if certificate['left_modular_values']!=[5,6,6,4,6,4,4]:
        raise SystemExit('Falsche Restwerte im Primzahlzertifikat.')
    if json.loads(subprocess.check_output([str(binary),'replay',str(target)],text=True))!=new:
        raise SystemExit('Modularer Vergleich nicht reproduzierbar.')
    print('  (x³−2)/(x³−2) und 1 über Q:',old['status'],'→',new['status'])
    print('  Primzahl:',certificate['left_modular_prime'],'– Restwerte:',certificate['left_modular_values'])
    print('  Logische Arbeit:',old['operations'],'→',new['operations'],'– V2-Replay geprüft.')

with tempfile.TemporaryDirectory(prefix='bruce-modular-research-demo-') as directory:
    print('\nCore 0.30 — modulare Q-Gruppen in Forschung und Erfahrungsplanung')
    for strategy in ['expression','portfolio-expression']:
        base=[str(binary),'research','--strategy',strategy,'--operators','-^/','--min','3','--max','3',
              '--max-nodes','9','--max-operations','10000000','--train',str(root/'examples/q-domain-zero-training.csv'),
              '--holdout',str(root/'examples/q-domain-zero-holdout.csv')]
        base+=['--planner','cached-grammar'] if strategy=='expression' else ['--expression-planner','cached-grammar','--max-power','3','--max-degree','3']
        old=json.loads(subprocess.check_output(base+['--equivalence','rational-q'],text=True))
        output=Path(directory)/(strategy+'.bruce')
        new=json.loads(subprocess.check_output(base+['--equivalence','rational-q-modular','--output',str(output)],text=True))
        if len(old['hypotheses'])-len(new['hypotheses'])!=1 or not new['domain_modular_certificates']:
            raise SystemExit('Kubischer Bereich wurde nicht modular zusammengeführt.')
        if old['stored_source_hypotheses']!=128 or new['stored_source_hypotheses']!=128 or new['status']!='INCOMPLETE':
            raise SystemExit('Ursprungsgrenze oder Teilbefund wurde verschleiert.')
        if json.loads(subprocess.check_output([str(binary),'replay',str(output)],text=True))!=new:
            raise SystemExit('Modularer Forschungsreplay weicht ab.')
        print(' ',strategy,':',len(old['hypotheses']),'→',len(new['hypotheses']),'Gruppen;',new['domain_modular_certificates'],'modulare Teilzertifikate.')
        print('  Alle 128 Ursprünge erhalten; Suche INCOMPLETE. Arbeit:',old['operations_used'],'→',new['operations_used'])
    qbase=[str(binary),'research','--strategy','portfolio-expression','--equivalence','rational-q-modular',
           '--expression-planner','cached-grammar','--min','1','--max','1','--max-power','2','--max-degree','2','--max-nodes','7']
    source=Path(directory)/'modular-source.bruce'
    subprocess.check_output(qbase+['--planner','adaptive-order','--train',str(root/'examples/expression-training.csv'),
        '--holdout',str(root/'examples/expression-holdout.csv'),'--output',str(source)],text=True)
    store=Path(directory)/'modular.experience'
    report=json.loads(subprocess.check_output([str(binary),'experience','add','--from-run',str(source),'--output',str(store)],text=True))
    target=qbase+['--train',str(root/'examples/expression-experience-target-training.csv'),
                  '--holdout',str(root/'examples/expression-experience-target-holdout.csv')]
    baseline=json.loads(subprocess.check_output(target+['--planner','adaptive-order'],text=True))
    learned_path=Path(directory)/'modular-learned.bruce'
    learned=json.loads(subprocess.check_output(target+['--planner','experience-order','--experience',str(store),'--output',str(learned_path)],text=True))
    if learned['hypotheses']!=baseline['hypotheses'] or learned['experience_votes']!=[0,0,0,0,1] or learned['strategy_order'][0]!='expression':
        raise SystemExit('Modulare Erfahrung verändert Befund oder ordnet nicht passend.')
    revoked=Path(directory)/'modular-revoked.experience'
    subprocess.check_output([str(binary),'experience','revoke','--store',str(store),'--id',report['records'][0]['id'],
                            '--reason','Demo Widerruf','--output',str(revoked)],text=True)
    fallback=json.loads(subprocess.check_output(target+['--planner','experience-order','--experience',str(revoked)],text=True))
    if fallback['experience_records_used'] or fallback['strategy_order']!=baseline['strategy_order']:
        raise SystemExit('Modularer Widerruf wirkt nicht.')
    selected=Path(directory)/'modular-selected.bruce'
    selection=json.loads(subprocess.check_output([str(binary),'compatibility','--rules',str(root/'examples/quadratic-fraction-selection.rules'),
        '--from-run',str(learned_path),'--at','0','--output',str(selected)],text=True))
    if selection['status']!='SELECTED' or selection['candidates'][0]['value']!='0':
        raise SystemExit('Modulare Auswahl liefert nicht null.')
    source.unlink();store.unlink();revoked.unlink()
    if json.loads(subprocess.check_output([str(binary),'replay',str(learned_path)],text=True))!=learned:
        raise SystemExit('Modularer Erfahrungsreplay benötigt externe Quellen.')
    learned_path.unlink()
    if json.loads(subprocess.check_output([str(binary),'replay',str(selected)],text=True))!=selection:
        raise SystemExit('Modularer Auswahlreplay benötigt Forschungsdatei.')
    print('  Vollständiger Erfahrungslauf V9:',learned['experience_votes'],'– Ausdruckssuche zuerst, gleiche Befunde.')
    print('  Widerruf, V28-Replay und Auswahl-Replay ohne ursprüngliche Quellen geprüft.')

with tempfile.TemporaryDirectory(prefix='bruce-domain-cache-demo-') as directory:
    print('\nCore 0.31 — bewiesene Bereichsgleichheit begrenzt wiederverwenden')
    base=[str(binary),'research','--strategy','expression','--planner','cached-grammar',
          '--equivalence','rational-q-modular','--operators','-^/','--min','3','--max','3',
          '--max-nodes','9','--max-operations','10000000','--train',str(root/'examples/q-domain-zero-training.csv'),
          '--holdout',str(root/'examples/q-domain-zero-holdout.csv')]
    old=json.loads(subprocess.check_output(base,text=True))
    output=Path(directory)/'cached-domains.bruce'
    new=json.loads(subprocess.check_output(base+['--domain-cache','equal-64','--output',str(output)],text=True))
    if old['hypotheses']!=new['hypotheses'] or new['status']!='INCOMPLETE' or new['stored_source_hypotheses']!=128:
        raise SystemExit('Bereichscache verändert Befunde oder Herkunft.')
    if not new['domain_cache_hits'] or new['operations_used']>=old['operations_used']:
        raise SystemExit('Referenzlauf spart keine logische Arbeit.')
    if json.loads(subprocess.check_output([str(binary),'replay',str(output)],text=True))!=new:
        raise SystemExit('Bereichscache-Replay weicht ab.')
    print('  Gleiche',len(new['hypotheses']),'Gruppen mit 128 Originalen; INCOMPLETE an Originalgrenze.')
    print('  Logische Arbeit:',old['operations_used'],'→',new['operations_used'],'– Cachetreffer:',new['domain_cache_hits'])
    print('  Cacheverwaltung:',new['domain_cache_operations'],'Einheiten bereits enthalten;',new['domain_cache_entries'],'Einträge,',new['domain_cache_key_bytes'],'Schlüsselbytes.')
    complete=Path(directory)/'complete.bruce'
    args=[str(binary),'research','--strategy','expression','--planner','cached-grammar','--equivalence','rational-q-modular',
          '--domain-cache','equal-64','--min','1','--max','1','--max-nodes','7',
          '--train',str(root/'examples/expression-training.csv'),'--holdout',str(root/'examples/expression-holdout.csv'),'--output',str(complete)]
    result=json.loads(subprocess.check_output(args,text=True))
    selected=Path(directory)/'selected.bruce'
    chosen=json.loads(subprocess.check_output([str(binary),'compatibility','--rules',str(root/'examples/quadratic-fraction-selection.rules'),
        '--from-run',str(complete),'--at','0','--output',str(selected)],text=True))
    if result['status']!='VERIFIED_ON_SAMPLES' or chosen['status']!='SELECTED' or chosen['candidates'][0]['value']!='0':
        raise SystemExit('Vollständiger Cachelauf oder Auswahl fehlt.')
    if json.loads(subprocess.check_output([str(binary),'replay',str(complete)],text=True))!=result:
        raise SystemExit('Vollständiger Cache-Replay weicht ab.')
    complete.unlink()
    if json.loads(subprocess.check_output([str(binary),'replay',str(selected)],text=True))!=chosen:
        raise SystemExit('Auswahl benötigt ursprünglichen Cachelauf.')
    print('  Vollständiger V9-Lauf, Auswahlwert 0 und Replay ohne ursprüngliche Forschungsdatei geprüft.')

with tempfile.TemporaryDirectory(prefix='bruce-domain-cache-portfolio-demo-') as directory:
    print('\nCore 0.32 — Bereichscache im Portfolio und eigene Erfahrungskontexte')
    base=[str(binary),'research','--strategy','portfolio-expression','--expression-planner','cached-grammar',
          '--equivalence','rational-q-modular','--train',str(root/'examples/q-domain-zero-training.csv'),
          '--holdout',str(root/'examples/q-domain-zero-holdout.csv')]
    for label,limits in [('vollständig',['--operators','+/','--min','0','--max','1','--max-nodes','3','--max-power','1','--max-degree','1']),
                         ('Teilbefund',['--operators','-^/','--min','3','--max','3','--max-nodes','9','--max-power','3','--max-degree','3','--max-operations','10000000'])]:
        old=json.loads(subprocess.check_output(base+limits,text=True))
        output=Path(directory)/(label+'.bruce')
        new=json.loads(subprocess.check_output(base+limits+['--domain-cache','equal-64','--output',str(output)],text=True))
        if old['hypotheses']!=new['hypotheses'] or old['stored_source_hypotheses']!=new['stored_source_hypotheses'] or old['status']!=new['status']:
            raise SystemExit('Portfolio-Bereichscache verändert den Befund.')
        if json.loads(subprocess.check_output([str(binary),'replay',str(output)],text=True))!=new:
            raise SystemExit('Portfolio-Bereichscache-Replay weicht ab.')
        print(' ',label,':',old['operations_used'],'→',new['operations_used'],'logische Einheiten;',new['status'])
        print('  Cachetreffer gesamt:',new['domain_cache_hits'],'– davon Vereinigung:',new['union_domain_cache_hits'])
    qbase=[str(binary),'research','--strategy','portfolio-expression','--equivalence','rational-q-modular','--domain-cache','equal-64',
           '--expression-planner','cached-grammar','--min','1','--max','1','--max-power','2','--max-degree','2','--max-nodes','7']
    source=Path(directory)/'modular-source.bruce'
    subprocess.check_output(qbase+['--planner','adaptive-order','--train',str(root/'examples/expression-training.csv'),
        '--holdout',str(root/'examples/expression-holdout.csv'),'--output',str(source)],text=True)
    store=Path(directory)/'modular.experience'
    report=json.loads(subprocess.check_output([str(binary),'experience','add','--from-run',str(source),'--output',str(store)],text=True))
    target=qbase+['--train',str(root/'examples/expression-experience-target-training.csv'),
                  '--holdout',str(root/'examples/expression-experience-target-holdout.csv')]
    baseline=json.loads(subprocess.check_output(target+['--planner','adaptive-order'],text=True))
    learned_path=Path(directory)/'modular-learned.bruce'
    learned=json.loads(subprocess.check_output(target+['--planner','experience-order','--experience',str(store),'--output',str(learned_path)],text=True))
    if learned['hypotheses']!=baseline['hypotheses'] or learned['experience_votes']!=[0,0,0,0,1] or learned['strategy_order'][0]!='expression':
        raise SystemExit('Modulare Erfahrung verändert Befund oder ordnet nicht passend.')
    revoked=Path(directory)/'modular-revoked.experience'
    subprocess.check_output([str(binary),'experience','revoke','--store',str(store),'--id',report['records'][0]['id'],
                            '--reason','Demo Widerruf','--output',str(revoked)],text=True)
    fallback=json.loads(subprocess.check_output(target+['--planner','experience-order','--experience',str(revoked)],text=True))
    if fallback['experience_records_used'] or fallback['strategy_order']!=baseline['strategy_order']:
        raise SystemExit('Modularer Widerruf wirkt nicht.')
    selected=Path(directory)/'modular-selected.bruce'
    selection=json.loads(subprocess.check_output([str(binary),'compatibility','--rules',str(root/'examples/quadratic-fraction-selection.rules'),
        '--from-run',str(learned_path),'--at','0','--output',str(selected)],text=True))
    if selection['status']!='SELECTED' or selection['candidates'][0]['value']!='0':
        raise SystemExit('Modulare Auswahl liefert nicht null.')
    source.unlink();store.unlink();revoked.unlink()
    if json.loads(subprocess.check_output([str(binary),'replay',str(learned_path)],text=True))!=learned:
        raise SystemExit('Modularer Erfahrungsreplay benötigt externe Quellen.')
    learned_path.unlink()
    if json.loads(subprocess.check_output([str(binary),'replay',str(selected)],text=True))!=selection:
        raise SystemExit('Modularer Auswahlreplay benötigt Forschungsdatei.')
    print('  Vollständiger Erfahrungslauf V10:',learned['experience_votes'],'– Ausdruckssuche zuerst, gleiche Befunde.')
    print('  Widerruf, V31-Replay und Auswahl-Replay ohne ursprüngliche Quellen geprüft.')


print('\nCore 0.33 — rationale Definitionslücken mit exakten Zeugen finden')
with tempfile.TemporaryDirectory(prefix='bruce-witness-demo-') as directory:
    for index, (polynomial, expected) in enumerate((('-8,0,0,1', '2'), ('-1,0,0,8', '1/2'))):
        base = [str(binary), 'compare', '--left-numerator', polynomial,
                '--left-denominator', polynomial, '--right-numerator', '1', '--right-denominator', '1']
        previous = json.loads(subprocess.check_output(base+['--domain-policy', 'modular'], text=True))
        path = Path(directory)/f'witness-{index}.bruce'
        proven = json.loads(subprocess.check_output(base+['--domain-policy', 'modular-witness', '--output', str(path)], text=True))
        if previous['status'] != 'UNDECIDED' or proven['status'] != 'PROVEN_DIFFERENT' or proven['domain_certificate']['witness'] != expected:
            raise SystemExit('Rationaler Lückenzeuge fehlt.')
        if json.loads(subprocess.check_output([str(binary), 'replay', str(path)], text=True)) != proven:
            raise SystemExit('Zeugen-Replay weicht ab.')
        print('  Nenner', polynomial, ':', previous['status'], '→', proven['status'], 'bei x =', expected)
    outside = json.loads(subprocess.check_output([str(binary), 'compare', '--left-numerator', '-4913,0,0,1',
        '--left-denominator', '-4913,0,0,1', '--right-numerator', '1', '--right-denominator', '1',
        '--domain-policy', 'modular-witness'], text=True))
    if outside['status'] != 'UNDECIDED' or outside['domain_certificate']['witness'] is not None:
        raise SystemExit('Begrenzte Suche wurde als Abwesenheitsbeweis ausgegeben.')
    print('  Lücke bei x = 17 liegt außerhalb der Suche:', outside['status'], '– kein falscher Gleichheitsbeweis.')

print('\nCore 0.34 — rationale Gegenbeispiele in der Ausdruckssuche')
with tempfile.TemporaryDirectory(prefix='bruce-witness-expression-') as directory:
    source=Path(directory)/'partial.bruce'
    run=json.loads(subprocess.check_output([str(binary),'research','--strategy','expression','--planner','cached-grammar',
        '--equivalence','rational-q-witness','--operators','-^/','--min','3','--max','3','--max-nodes','9',
        '--max-results','256','--max-operations','10000000','--train',str(root/'examples/witness-zero-training.csv'),
        '--holdout',str(root/'examples/witness-zero-holdout.csv'),'--output',str(source)],text=True))
    if run['domain_bounded_witnesses']!=4 or run['stored_source_hypotheses']!=256 or run['status']!='INCOMPLETE':
        raise SystemExit('Rationale Gegenbeispiele oder Quellenlimit fehlen.')
    if json.loads(subprocess.check_output([str(binary),'replay',str(source)],text=True))!=run:
        raise SystemExit('Ausdruckszeugen-Replay weicht ab.')
    print(' ',run['domain_bounded_witnesses'],'abgeschlossene begrenzte Gegenbeispiele;',run['stored_source_hypotheses'],
          'Originaltreffer;',run['status'],'an Ergebnisgrenze.')
    print(' ',next(event for event in run['trace'] if 'domain_witness=' in event))
    complete=Path(directory)/'complete.bruce';selection=Path(directory)/'selected.bruce'
    research=json.loads(subprocess.check_output([str(binary),'research','--strategy','expression','--planner','cached-grammar',
        '--equivalence','rational-q-witness','--min','1','--max','1','--max-nodes','7',
        '--train',str(root/'examples/expression-training.csv'),'--holdout',str(root/'examples/expression-holdout.csv'),
        '--output',str(complete)],text=True))
    if research['status']!='VERIFIED_ON_SAMPLES':raise SystemExit('Vollständiger Zeugenmodus-Lauf fehlt.')
    choice=json.loads(subprocess.check_output([str(binary),'compatibility','--rules',str(root/'examples/quadratic-fraction-selection.rules'),
        '--from-run',str(complete),'--at','0','--output',str(selection)],text=True))
    if choice['status']!='SELECTED' or choice['candidates'][0]['value']!='0':raise SystemExit('Zeugenmodus-Auswahl fehlt.')
    pole=subprocess.run([str(binary),'compatibility','--rules',str(root/'examples/quadratic-fraction-selection.rules'),
        '--from-run',str(complete),'--at','-1'],capture_output=True,text=True)
    if pole.returncode!=2:raise SystemExit('Polstelle wurde akzeptiert.')
    complete.unlink()
    if json.loads(subprocess.check_output([str(binary),'replay',str(selection)],text=True))!=choice:
        raise SystemExit('Zeugenmodus-Auswahlreplay benötigt ursprüngliche Quelle.')
    print('  Vollständiger V10-Lauf, Auswahlwert 0, gesperrte Polstelle −1 und Replay ohne Quelldatei geprüft.')

print('\nCore 0.35 — Gegenbeispiele im Portfolio und eigene Erfahrungskontexte')
with tempfile.TemporaryDirectory(prefix='bruce-witness-portfolio-') as directory:
    def invoke(*args):
        return json.loads(subprocess.check_output([str(binary),*map(str,args)],text=True))
    base=['research','--strategy','portfolio-expression','--equivalence','rational-q-witness',
          '--expression-planner','cached-grammar']
    partial=Path(directory)/'partial.bruce'
    run=invoke(*base,'--operators','-^/','--min',3,'--max',3,'--max-power',3,'--max-degree',3,'--max-nodes',9,
        '--max-results',256,'--max-operations',10000000,'--train',root/'examples/witness-zero-training.csv',
        '--holdout',root/'examples/witness-zero-holdout.csv','--output',partial)
    if run['domain_bounded_witnesses']!=4 or run['stored_source_hypotheses']!=256 or invoke('replay',partial)!=run:
        raise SystemExit('Portfolio-Gegenbeispiele oder Replay fehlen.')
    print(' ',run['domain_bounded_witnesses'],'Gegenbeispiele;',run['operations_used'],'Einheiten;',run['status'])
    small=base+['--min',1,'--max',1,'--max-power',2,'--max-degree',2,'--max-nodes',7]
    source=Path(directory)/'source.bruce';store=Path(directory)/'store.experience';target=Path(directory)/'target.bruce'
    invoke(*small,'--planner','adaptive-order','--train',root/'examples/expression-training.csv',
        '--holdout',root/'examples/expression-holdout.csv','--output',source)
    experience=invoke('experience','add','--from-run',source,'--output',store)
    learned=invoke(*small,'--planner','experience-order','--experience',store,
        '--train',root/'examples/expression-experience-target-training.csv',
        '--holdout',root/'examples/expression-experience-target-holdout.csv','--output',target)
    if experience['engine']!='bruce-experience-v11' or learned['experience_votes']!=[0,0,0,0,1]:
        raise SystemExit('V11-Erfahrungsplanung fehlt.')
    revoked=Path(directory)/'revoked.experience'
    invoke('experience','revoke','--store',store,'--id',experience['records'][0]['id'],'--reason','Demo-Widerruf','--output',revoked)
    fallback=invoke(*small,'--planner','experience-order','--experience',revoked,
        '--train',root/'examples/expression-experience-target-training.csv','--holdout',root/'examples/expression-experience-target-holdout.csv')
    if fallback['experience_records_used']!=0:raise SystemExit('V11-Widerruf wirkt nicht.')
    selected=Path(directory)/'selected.bruce'
    choice=invoke('compatibility','--rules',root/'examples/quadratic-fraction-selection.rules','--from-run',target,'--at',0,'--output',selected)
    if choice['status']!='SELECTED' or choice['candidates'][0]['value']!='0':raise SystemExit('V34-Auswahl fehlt.')
    source.unlink();store.unlink();revoked.unlink()
    if invoke('replay',target)!=learned:raise SystemExit('V34 benötigt ursprüngliche Quellen.')
    target.unlink()
    if invoke('replay',selected)!=choice:raise SystemExit('V34-Auswahl benötigt Forschungsdatei.')
    print('  V11-Erfahrung, Widerruf, V34-Replay und Auswahlwert 0 ohne ursprüngliche Quellen geprüft.')

print('\nCore 0.36 — Grid-Einzelauftrag mit Export und Replay')
with tempfile.TemporaryDirectory(prefix='bruce-grid-demo-') as directory:
    for mode in ('exact','modular','metric'):
        path=Path(directory)/(mode+'.bruce')
        args=[str(binary),'grid','--path','/'.join(['2,6']*11+['4,3']),'--mode',mode,'--output',str(path)]
        if mode=='modular':args+=['--modulus','7']
        result=json.loads(subprocess.check_output(args,text=True))
        expected=7 if mode=='metric' else '1' if mode=='modular' else '64'
        if result.get('bit_length',result.get('value'))!=expected or result['evaluations']!=1:
            raise SystemExit('Grid-Blattwert oder Evaluationszähler falsch.')
        if json.loads(subprocess.check_output([str(binary),'replay',str(path)],text=True))!=result:
            raise SystemExit('Grid-Replay weicht ab.')
        print(' ',mode,':',expected,'– Tiefe 12, eine Blattauswertung, Replay identisch.')
    path=Path(directory)/'stopped.bruce'
    stopped=json.loads(subprocess.check_output([str(binary),'grid','--path','2,6/0,0','--zero-zero','stop','--output',str(path)],text=True))
    if stopped['status']!='STOPPED' or stopped['evaluations']!=0 or json.loads(subprocess.check_output([str(binary),'replay',str(path)],text=True))!=stopped:
        raise SystemExit('Grid-Sentinel-Replay fehlt.')
    print('  Sentinel: BRANCH-Stopp ohne Auswertung exportiert und replayt.')

print('\nCore 0.37 — GridSpec und begrenzter Cache')
with tempfile.TemporaryDirectory(prefix='bruce-grid-cache-demo-') as directory:
    path=Path(directory)/'batch.bruce'
    args=[str(binary),'grid-batch','--paths','2,6;02,06;4,3;1,1/2,6;2,6']
    cached=json.loads(subprocess.check_output(args+['--output',str(path)],text=True))
    uncached=json.loads(subprocess.check_output(args+['--cache-bytes','0'],text=True))
    if [q['result'] for q in cached['queries']]!=[q['result'] for q in uncached['queries']]:
        raise SystemExit('Grid-Cache verändert Ergebnisse.')
    if (cached['evaluations'],uncached['evaluations'],cached['cache']['hits'])!=(3,5,2):
        raise SystemExit('Grid-Cache trennt Adressen oder Wiederholungen falsch.')
    if json.loads(subprocess.check_output([str(binary),'replay',str(path)],text=True))!=cached:
        raise SystemExit('Grid-Stapel-Replay weicht ab.')
    stopped=json.loads(subprocess.check_output([str(binary),'grid-batch','--paths','0,0;2,6;0,0;2,6','--zero-zero','stop'],text=True))
    if (stopped['evaluations'],stopped['cache']['hits'],stopped['cache']['bypasses'])!=(1,1,2):
        raise SystemExit('Grid-Stopp greift auf Nachbaranfragen über.')
    print('  Fünf Abfragen, drei Adressen: 3 statt 5 Potenzberechnungen; Ergebnisse identisch.')
    print('  Zwei lokale Stopps beeinflussen Nachbarn nicht; Cacheentscheidungen replaybar.')
    print('  Abgerechnete Cachebytes:',cached['cache']['bytes'],'von',cached['cache_byte_limit'])

print('\nCore 0.38 — Grid → Forschung → Auswahl mit vollständiger Herkunft')
with tempfile.TemporaryDirectory(prefix='bruce-grid-research-demo-') as directory:
    def grid_invoke(*args):
        return json.loads(subprocess.check_output([str(binary),*map(str,args)],text=True))
    train=Path(directory)/'train.bruce';hold=Path(directory)/'hold.bruce'
    research_path=Path(directory)/'research.bruce';selection_path=Path(directory)/'selection.bruce'
    grid_invoke('grid-batch','--paths','0,2;1,2;2,2','--output',train)
    grid_invoke('grid-batch','--paths','3,2;4,2','--output',hold)
    found=grid_invoke('research','--strategy','polynomial','--domain','rational','--max-degree',2,
                      '--grid-train',train,'--grid-holdout',hold,'--output',research_path)
    if found['status']!='VERIFIED_ON_SAMPLES' or found['research']['hypotheses'][0]['coefficients']!=['0','0','1']:
        raise SystemExit('Grid-Forschung hat x² nicht auf den Samples bestätigt.')
    selected=grid_invoke('compatibility','--rules',root/'examples/quadratic-fraction-selection.rules',
                         '--from-run',research_path,'--at',5,'--output',selection_path)
    if selected['status']!='SELECTED' or selected['candidates'][0]['value']!='25':
        raise SystemExit('Grid-Forschungs-Auswahl stimmt nicht.')
    train.unlink();hold.unlink()
    if grid_invoke('replay',research_path)!=found:raise SystemExit('Forschung benötigt ursprüngliche Grid-Dateien.')
    research_path.unlink()
    if grid_invoke('replay',selection_path)!=selected:raise SystemExit('Auswahl benötigt ursprüngliche Forschungsdatei.')
    print('  Grid-Paare → x²; zusätzliche Prüfwerte 9 und 16 bestätigen die Samples.')
    print('  Hypothese bei x=5: 25; Forschung und Auswahl ohne Quelldateien replayt.')

print('\nCore 0.39 — Exponentenrichtung mit ehrlichem Gegenbefund')
with tempfile.TemporaryDirectory(prefix='bruce-grid-axis-demo-') as directory:
    def axis_invoke(*args):
        return json.loads(subprocess.check_output([str(binary),*map(str,args)],text=True))
    for base in (1,2):
        train=Path(directory)/f'train-{base}';hold=Path(directory)/f'hold-{base}';result_path=Path(directory)/f'research-{base}'
        axis_invoke('grid-batch','--paths',';'.join(f'{base},{y}' for y in (0,1,2)),'--output',train)
        axis_invoke('grid-batch','--paths',';'.join(f'{base},{y}' for y in (3,4)),'--output',hold)
        result=axis_invoke('research','--strategy','polynomial','--domain','rational','--max-degree',2,
                          '--grid-axis','y','--grid-train',train,'--grid-holdout',hold,'--output',result_path)
        expected='VERIFIED_ON_SAMPLES' if base==1 else 'REFUTED'
        if result['status']!=expected or result['grid_origin']['base']!=base:
            raise SystemExit('Exponentenprojektion oder Gegenbefund falsch.')
        train.unlink();hold.unlink()
        if axis_invoke('replay',result_path)!=result:raise SystemExit('Y-Projektion benötigt ursprüngliche Quellen.')
        print('  Basis',base,':',expected,'– Y als Eingabe, vollständiger Replay ohne Quelldateien.')

print('\nCore 0.40 — Gemeinsamer logischer Import-/Forschungsrahmen')
with tempfile.TemporaryDirectory(prefix='bruce-grid-budget-demo-') as directory:
    def budget_invoke(*args):
        return json.loads(subprocess.check_output([str(binary),*map(str,args)],text=True))
    train=Path(directory)/'train';hold=Path(directory)/'hold'
    budget_invoke('grid-batch','--paths','0,2;1,2;2,2','--output',train)
    budget_invoke('grid-batch','--paths','3,2;4,2','--output',hold)
    args=['research','--strategy','polynomial','--domain','rational','--max-degree',2,'--grid-train',train,'--grid-holdout',hold]
    reference=budget_invoke(*args);source_bytes=train.stat().st_size+hold.stat().st_size
    work=reference['research']['operations_used'];runs=[]
    for remaining in (work-1,work):
        path=Path(directory)/f'run-{remaining}'
        run=budget_invoke(*args,'--max-total-operations',source_bytes+remaining,'--output',path)
        expected='INCOMPLETE' if remaining<work else reference['status']
        if run['status']!=expected or run['work_budget']['used']>source_bytes+remaining:
            raise SystemExit('Gesamtbudget oder Grenzstatus falsch.')
        runs.append((path,run))
        print('  Quellen:',source_bytes,'+ Forschungsrahmen:',remaining,'→',run['status'])
    train.unlink();hold.unlink()
    for path,run in runs:
        if budget_invoke('replay',path)!=run:raise SystemExit('Budget-Replay benötigt ursprüngliche Quellen.')
    print('  Beide Budgetgrenzen ohne ursprüngliche Quelldateien replayt.')

print('\nCore 0.41 — Grid-Teilbefunde mit Abfragegrenze')
with tempfile.TemporaryDirectory(prefix='bruce-grid-partial-demo-') as directory:
    path=Path(directory)/'partial.bruce'
    args=[str(binary),'grid-batch','--paths','2,6;02,06;0,0;4,3;2,6','--zero-zero','stop']
    full=json.loads(subprocess.check_output(args,text=True))
    partial=json.loads(subprocess.check_output(args+['--max-query-steps','3','--output',str(path)],text=True))
    if partial['status']!='INCOMPLETE' or partial['queries']!=full['queries'][:3] or partial['evaluations']!=1:
        raise SystemExit('Grid-Präfix oder Cachezählung falsch.')
    if json.loads(subprocess.check_output([str(binary),'replay',str(path)],text=True))!=partial:
        raise SystemExit('Grid-Teilreplay weicht ab.')
    print('  3 von 5 Abfragen: eine Berechnung, ein Cachetreffer, ein lokaler Stopp.')
    print('  INCOMPLETE / QueryBudgetExceeded; ursprüngliche Liste und Teilbefund replaybar.')

print('\nCore 0.42 — Grid-Fortsetzung mit vollständiger Herkunft')
with tempfile.TemporaryDirectory(prefix='bruce-grid-resume-demo-') as directory:
    def resume_invoke(*args):
        return json.loads(subprocess.check_output([str(binary),*map(str,args)],text=True))
    parent=Path(directory)/'parent';child=Path(directory)/'child';final=Path(directory)/'final'
    resume_invoke('grid-batch','--paths','2,6;02,06;0,0;4,3;2,6','--zero-zero','stop','--max-query-steps',1,'--output',parent)
    first=resume_invoke('grid-resume','--from-run',parent,'--additional-queries',2,'--output',child)
    last=resume_invoke('grid-resume','--from-run',child,'--additional-queries',2,'--output',final)
    assert (first['status'],first['audit_evaluations'],first['new_evaluations'])==('INCOMPLETE',2,0)
    assert (last['status'],last['generation'],last['audit_evaluations'],last['new_evaluations'])==('COMPLETED',2,3,1)
    parent.unlink();child.unlink();assert resume_invoke('replay',final)==last
    print('  Generation 2: fertig; 3 Auditberechnungen, 1 neue Berechnung.')
    print('  Replay ohne Vorgängerdateien identisch; Cachehistorie vollständig erhalten.')

print('\nCore 0.43 — Grid-Forschung wird geprüfte Erfahrung')
with tempfile.TemporaryDirectory(prefix='bruce-grid-experience-demo-') as directory:
    def experience_invoke(*args):
        return json.loads(subprocess.check_output([str(binary),*map(str,args)],text=True))
    train=Path(directory)/'train';hold=Path(directory)/'hold';source=Path(directory)/'source';store=Path(directory)/'store'
    config=['--strategy','portfolio-rational','--max-degree','2','--min','-2','--max','2','--max-power','2','--max-denominator','1']
    experience_invoke('grid-batch','--paths','0,2;1,2;2,2','--output',train)
    experience_invoke('grid-batch','--paths','3,2','--output',hold)
    experience_invoke('research',*config,'--grid-train',train,'--grid-holdout',hold,'--output',source)
    report=experience_invoke('experience','add','--from-run',source,'--output',store)
    assert report['engine']=='bruce-experience-v12' and report['records'][0]['grid_origin']
    t=Path(directory)/'next.csv';h=Path(directory)/'check.csv';result=Path(directory)/'result'
    t.write_text('x,y\n4,16\n5,25\n6,36\n');h.write_text('x,y\n7,49\n')
    run=experience_invoke('research',*config,'--train',t,'--holdout',h,'--planner','experience-order','--experience',store,'--output',result)
    assert run['experience_records_used']==1 and run['experience_votes']==report['records'][0]['votes']
    for p in Path(directory).iterdir():
        if p!=result:p.unlink()
    assert experience_invoke('replay',result)==run
    print('  Geprüfte Grid-Quelle liefert Strategiestimmen:',run['experience_votes'])
    print('  Späterer Forschungslauf replayt ohne ursprüngliche Quellen und Storedatei.')

print('\nCore 0.44 — Experience-Korrektur mit erhaltenem Vorgänger')
with tempfile.TemporaryDirectory(prefix='bruce-experience-replace-demo-') as directory:
    def replace_invoke(*args):
        return json.loads(subprocess.check_output([str(binary),*map(str,args)],text=True))
    t=Path(directory)/'train.csv';h=Path(directory)/'hold.csv'
    t.write_text('x,y\n0,0\n1,1\n2,4\n')
    sources=[]
    for x in (3,4):
        h.write_text(f'x,y\n{x},{x*x}\n');source=Path(directory)/f'source-{x}'
        replace_invoke('research','--strategy','portfolio-rational','--min',-2,'--max',2,'--max-power',2,'--max-degree',2,'--max-denominator',1,'--train',t,'--holdout',h,'--output',source)
        sources.append(source)
    old=Path(directory)/'old';new=Path(directory)/'new'
    report=replace_invoke('experience','add','--from-run',sources[0],'--output',old);target=report['records'][0]['id']
    corrected=replace_invoke('experience','replace','--store',old,'--id',target,'--reason','Weiterer Holdout-Lauf','--from-run',sources[1],'--output',new)
    assert corrected['active_records']==1 and not corrected['records'][0]['active']
    assert corrected['records'][1]['replaces']==target
    for p in Path(directory).iterdir():
        if p!=new:p.unlink()
    assert replace_invoke('experience','inspect',new)==corrected
    print('  Vorgänger inaktiv, geprüfter Nachfolger aktiv; nur eine Erfahrungsstimme je Familie.')
    print('  Grund und vollständige Quellen bleiben ohne Originaldateien prüfbar.')

print('\nCore 0.45 — Gemeinsamer Rahmen für Grid, Experience und neue Suche')
with tempfile.TemporaryDirectory(prefix='bruce-grid-experience-budget-demo-') as directory:
    def combined_invoke(*args):
        return json.loads(subprocess.check_output([str(binary),*map(str,args)],text=True))
    t=Path(directory)/'train';h=Path(directory)/'hold';source=Path(directory)/'source';store=Path(directory)/'store'
    config=['--strategy','portfolio-rational','--min','-2','--max','2','--max-power','2','--max-degree','2','--max-denominator','1']
    combined_invoke('grid-batch','--paths','0,2;1,2;2,2','--output',t)
    combined_invoke('grid-batch','--paths','3,2','--output',h)
    combined_invoke('research',*config,'--grid-train',t,'--grid-holdout',h,'--output',source)
    combined_invoke('experience','add','--from-run',source,'--output',store)
    args=['research',*config,'--grid-train',t,'--grid-holdout',h,'--planner','experience-order','--experience',store]
    full=combined_invoke(*args);work=full['research']['operations_used']
    imports=t.stat().st_size+h.stat().st_size+store.stat().st_size;runs=[]
    for remaining in (1,work):
        path=Path(directory)/f'run-{remaining}'
        out=combined_invoke(*args,'--max-total-operations',imports+remaining,'--output',path)
        assert out['work_budget']['experience_import_operations']==store.stat().st_size
        assert out['work_budget']['used']==imports+out['research']['operations_used']
        assert out['status']==('INCOMPLETE' if remaining==1 else full['status'])
        runs.append((path,out))
        print('  Importkosten',imports,'+ Suchrahmen',remaining,'→',out['status'])
    for path in (t,h,source,store):path.unlink()
    for path,out in runs:assert combined_invoke('replay',path)==out
    print('  Beide V4-Befunde ohne ursprüngliche Grid-/Experience-Dateien replayt.')

print('\nCore 0.46 — Gemeinsamer Forschungsvertrag')
contract=json.loads(subprocess.check_output([str(binary),'contracts'],text=True))
assert contract['contract']=='exact-research-task-v1'
assert len(contract['strategies'])==10 and not contract['dynamic_plugins']
assert not contract['gates']['learn_promotion'] and not contract['gates']['publish']
print('  10 eingebaute Strategievarianten mit gemeinsamen Planerregeln.')
print('  Exact-Auftragsprüfung und Forschungsübergänge verwenden gemeinsame Verträge.')

print('\nCore 0.47 — Gemeinsamer Einstieg für acht Laufarten')
application_contract=json.loads(subprocess.check_output([str(binary),'task-contracts'],text=True))
assert application_contract['contract']=='application-task-v1'
assert len(application_contract['tasks'])==8 and not application_contract['unified_budget_units']
print('  Forschung, Grid, Fortsetzung, Vergleich und Z laufen über typisierte Adapter.')
print('  Die vorangegangenen Demos verwenden bereits diesen gemeinsamen CLI-Einstieg.')

print('\nCore 0.48 — Gemeinsamer Ergebnis- und Replay-Vertrag')
with tempfile.TemporaryDirectory(prefix='bruce-inspect-demo-') as directory:
    path=Path(directory)/'partial'
    subprocess.check_output([str(binary),'grid-batch','--paths','2,6;4,3','--max-query-steps','1','--output',str(path)])
    summary=json.loads(subprocess.check_output([str(binary),'inspect-run',str(path)],text=True))
    assert summary['replay_checked'] and summary['lifecycle']=='INCOMPLETE' and summary['kind']=='grid-batch'
    print('  Vollständig replaygeprüft:',summary['kind'],summary['lifecycle'],summary['stop_reason'])
    print('  Ausführungsabschluss bleibt vom mathematischen Wahrheitsstatus getrennt.')

print('\nCore 0.49 — Optionale gemeinsame Ausführungsphasen')
with tempfile.TemporaryDirectory(prefix='bruce-session-demo-') as directory:
    path=Path(directory)/'partial'
    out=json.loads(subprocess.check_output([str(binary),'grid-batch','--paths','2,6;4,3','--max-query-steps','1','--verification','replay','--output',str(path)],text=True))
    assert out['execution']['phases']==['CREATED','EXECUTING','SERIALIZING','REPLAYING','READY']
    assert out['execution']['replay_verified'] and not out['execution']['result_complete']
    assert json.loads(subprocess.check_output([str(binary),'replay',str(path)],text=True))==out['result']
    print('  Ausgeführt, serialisiert, replaygeprüft, freigegeben; der Befund bleibt INCOMPLETE.')
    print('  Das exportierte native Bundle behält sein bisheriges Format.')

print('\nCore 0.50 — Strukturierter Fehlerverlauf')
with tempfile.TemporaryDirectory(prefix='bruce-failure-demo-') as directory:
    path=Path(directory)/'must-not-exist'
    failed=subprocess.run([str(binary),'grid','--path','invalid','--verification','replay','--output',str(path)],capture_output=True,text=True)
    report=json.loads(failed.stderr)
    assert failed.returncode==2 and not failed.stdout and not path.exists()
    assert report['execution']['phases']==['CREATED','EXECUTING','FAILED']
    assert report['execution']['failed_at']=='EXECUTING'
    assert report['execution']['cause']=='InvalidGridPath: expected x,y/x,y'
    print('  Fehlerphase:',report['execution']['failed_at'],'— Ursache:',report['execution']['cause'])
    print('  Kein Ergebnisexport; der fehlgeschlagene Ablauf bleibt ausdrücklich FAILED.')

print('\nCore 0.51 — Optionales Bytebudget vor dem Replay')
with tempfile.TemporaryDirectory(prefix='bruce-replay-budget-demo-') as directory:
    source=Path(directory)/'source';target=Path(directory)/'target'
    args=[str(binary),'grid','--path','2,6','--verification','replay']
    plain=json.loads(subprocess.check_output([*args,'--output',str(source)],text=True))
    size=source.stat().st_size
    bounded=json.loads(subprocess.check_output([*args,'--max-replay-bytes',str(size),'--output',str(target)],text=True))
    assert bounded['result']==plain['result'] and source.read_bytes()==target.read_bytes()
    assert bounded['execution']['verification_budget']=={'max_replay_bytes':size,'replay_bytes':size}
    failed=subprocess.run([*args,'--max-replay-bytes',str(size-1)],capture_output=True,text=True)
    assert failed.returncode==2 and json.loads(failed.stderr)['execution']['failed_at']=='REPLAYING'
    print('  Exakt',size,'Bytes zugelassen; ein Byte weniger stoppt vor dem Replay.')
    print('  Die Grenze betrifft Prüfdaten, nicht Rechenzeit oder Gesamtspeicher.')

print('\nCore 0.52 — Optionale Abbruchpunkte zwischen Prüfphasen')
out=json.loads(subprocess.check_output([str(binary),'grid-batch','--paths','2,6;4,3','--max-query-steps','1',
    '--verification','replay','--verification-cancellation','phase-boundaries','--max-replay-bytes','10000'],text=True))
assert out['execution']['contract']=='application-execution-v3'
assert out['execution']['verification_cancellation']=='phase-boundaries'
assert out['execution']['replay_verified'] and not out['execution']['result_complete']
print('  Drei Prüfgrenzen aktiv; ohne Abbruchsignal wird der Teilbefund vollständig replaygeprüft.')
print('  Laufende Serialisierung und Replay bleiben innerhalb ihrer Phase atomar.')

print('\nCore 0.53 — Interne Grid-Replaypunkte')
with tempfile.TemporaryDirectory(prefix='bruce-grid-replay-demo-') as directory:
    parent=Path(directory)/'parent';child=Path(directory)/'child'
    flags=['--verification','replay','--verification-cancellation','grid-query-boundaries']
    first=json.loads(subprocess.check_output([str(binary),'grid-batch','--paths','2,6;4,3;3,2','--max-query-steps','1',*flags,'--output',str(parent)],text=True))
    second=json.loads(subprocess.check_output([str(binary),'grid-resume','--from-run',str(parent),'--additional-queries','1',*flags,'--output',str(child)],text=True))
    assert first['execution']['contract']==second['execution']['contract']=='application-execution-v4'
    assert second['execution']['verification_cancellation']=='grid-query-boundaries'
    assert json.loads(subprocess.check_output([str(binary),'replay',str(child)],text=True))==second['result']
    print('  Stapel und Fortsetzung vollständig geprüft; innere Abbruchpunkte sind aktiviert.')
    print('  Ohne Abbruchsignal bleiben die nativen Bundlebytes und Teilbefunde erhalten.')

print('\nCore 0.54 — Forschungsreplay mit Suchcheckpoints')
with tempfile.TemporaryDirectory(prefix='bruce-research-replay-demo-') as directory:
    target=Path(directory)/'research'
    out=json.loads(subprocess.check_output([str(binary),'research','--train',str(root/'examples/affine-training.csv'),
        '--holdout',str(root/'examples/affine-holdout.csv'),'--domain','rational','--verification','replay',
        '--verification-cancellation','research-checkpoints','--output',str(target)],text=True))
    assert out['execution']['contract']=='application-execution-v5'
    assert out['execution']['verification_cancellation']=='research-checkpoints'
    assert json.loads(subprocess.check_output([str(binary),'replay',str(target)],text=True))==out['result']
    print('  Mathematischer Befund vollständig reproduziert; innere Suchcheckpoints sind aktiviert.')
    print('  Einzeloperationen und Erfahrungs-Quellaudits bleiben atomar.')

print('\nCore 0.55 — Zusammengesetzter Grid-Forschungsreplay')
with tempfile.TemporaryDirectory(prefix='bruce-composed-replay-demo-') as directory:
    train=Path(directory)/'train';hold=Path(directory)/'hold';target=Path(directory)/'research'
    subprocess.check_output([str(binary),'grid-batch','--paths','0,2;1,2;2,2','--output',str(train)])
    subprocess.check_output([str(binary),'grid-batch','--paths','3,2','--output',str(hold)])
    out=json.loads(subprocess.check_output([str(binary),'research','--grid-train',str(train),'--grid-holdout',str(hold),
        '--domain','rational','--strategy','polynomial','--planner','interpolate','--max-degree','2',
        '--verification','replay','--verification-cancellation','research-checkpoints','--output',str(target)],text=True))
    assert out['execution']['contract']=='application-execution-v5'
    assert json.loads(subprocess.check_output([str(binary),'replay',str(target)],text=True))==out['result']
    print('  Gridquellen, Samplebindung und Formelforschung vollständig reproduziert.')
    print('  Dieselbe Abbruchanforderung erreicht ihre inneren Replay-Prüfpunkte.')

print('\nCore 0.56 — Bruchvergleichsreplay mit Rechencheckpoints')
with tempfile.TemporaryDirectory(prefix='bruce-comparison-replay-demo-') as directory:
    target=Path(directory)/'comparison'
    out=json.loads(subprocess.check_output([str(binary),'compare','--left-numerator','-1,0,1','--left-denominator','-1,1',
        '--right-numerator','1,1','--right-denominator','1','--domain-policy','modular-witness',
        '--verification','replay','--verification-cancellation','comparison-checkpoints','--output',str(target)],text=True))
    assert out['execution']['contract']=='application-execution-v6'
    assert out['execution']['verification_cancellation']=='comparison-checkpoints'
    assert json.loads(subprocess.check_output([str(binary),'replay',str(target)],text=True))==out['result']
    print('  Bruchvergleich vollständig reproduziert; Definitionslücken bleiben Teil der Aussage.')
    print('  Ein Prüfungsabbruch wird nicht als mathematischer Grenzfall ausgegeben.')

print('\nCore 0.57 — Z-Kompatibilitätsreplay mit inneren Prüfpunkten')
with tempfile.TemporaryDirectory(prefix='bruce-compatibility-replay-demo-') as directory:
    root=Path(directory);rules=root/'rules';candidates=root/'candidates';target=root/'selection'
    rules.write_text('BRUCE_RULES_V1\nsnapshot demo\nrule even positive value mod_eq 2 0 5 manual -\n')
    candidates.write_text('id,domain,layer,x,y,value\n1,math,0,0,0,2\n2,math,0,1,0,3\n')
    out=json.loads(subprocess.check_output([str(binary),'compatibility','--rules',str(rules),'--candidates',str(candidates),
        '--verification','replay','--verification-cancellation','compatibility-checkpoints','--output',str(target)],text=True))
    assert out['execution']['contract']=='application-execution-v7'
    assert out['result']['selected_ids']==['1']
    assert json.loads(subprocess.check_output([str(binary),'replay',str(target)],text=True))==out['result']
    print('  Gerader Kandidat ausgewählt und vollständig reproduziert.')
    print('  Die Replayprüfung kann an inneren Kandidatengrenzen abbrechen.')

print('\nCore 0.58 — Erfahrungsquellen mit inneren Replayprüfpunkten')
with tempfile.TemporaryDirectory(prefix='bruce-experience-replay-demo-') as directory:
    root=Path(directory);train=root/'train';hold=root/'hold';source=root/'source';store=root/'store';target=root/'learned'
    train.write_text('x,y\n0,0\n1,1\n2,2\n');hold.write_text('x,y\n3,3\n')
    common=[str(binary),'research','--train',str(train),'--holdout',str(hold),'--domain','rational',
        '--strategy','portfolio-rational','--max-denominator','1','--min','-1','--max','1','--max-power','2','--max-degree','2']
    subprocess.check_output(common+['--planner','portfolio','--output',str(source)],text=True)
    subprocess.check_output([str(binary),'experience','add','--from-run',str(source),'--output',str(store)],text=True)
    train.write_text('x,y\n4,4\n5,5\n6,6\n');hold.write_text('x,y\n7,7\n')
    out=json.loads(subprocess.check_output(common+['--planner','experience-order','--experience',str(store),
        '--verification','replay','--verification-cancellation','research-checkpoints','--output',str(target)],text=True))
    assert out['execution']['contract']=='application-execution-v5'
    assert out['result']['experience_records_used']==1
    assert json.loads(subprocess.check_output([str(binary),'replay',str(target)],text=True))==out['result']
    print('  Erfahrungsquelle geprüft und ihre Stimme für die Strategiereihenfolge verwendet.')
    print('  Dieselbe Abbruchanforderung erreicht nun auch die Quellenprüfung im Replay.')

print('\nCore 0.59 — Äußere Bundle-Prüfsumme mit Abbruchpunkten')
with tempfile.TemporaryDirectory(prefix='bruce-serialization-demo-') as directory:
    root=Path(directory);plain=root/'plain';checked=root/'checked'
    common=[str(binary),'grid-batch','--paths','2,6;4,3']
    old=json.loads(subprocess.check_output(common+['--output',str(plain)],text=True))
    out=json.loads(subprocess.check_output(common+['--verification','replay',
        '--serialization-cancellation','digest-checkpoints','--verification-cancellation','grid-query-boundaries',
        '--output',str(checked)],text=True))
    assert out['execution']['contract']=='application-execution-v8'
    assert out['execution']['serialization_cancellation']=='digest-checkpoints'
    assert out['result']==old and plain.read_bytes()==checked.read_bytes()
    assert json.loads(subprocess.check_output([str(binary),'replay',str(checked)],text=True))==old
    print('  Prüfsumme blockweise berechnet; native Bundlebytes identisch.')
    print('  SERIALIZING besitzt nun eigene Abbruchpunkte vor dem Export.')

print('\nCore 0.60 — Grid-Evidenz mit Abbruchpunkten im Inhaltsaufbau')
with tempfile.TemporaryDirectory(prefix='bruce-grid-record-demo-') as directory:
    root=Path(directory);partial=root/'partial';resumed=root/'resumed'
    mode=['--verification','replay','--serialization-cancellation','grid-record-checkpoints',
        '--verification-cancellation','grid-query-boundaries']
    first=json.loads(subprocess.check_output([str(binary),'grid-batch','--paths','0,1;1,1;2,1',
        '--max-query-steps','1',*mode,'--output',str(partial)],text=True))
    final=json.loads(subprocess.check_output([str(binary),'grid-resume','--from-run',str(partial),
        '--additional-queries','2',*mode,'--output',str(resumed)],text=True))
    assert first['execution']['contract']==final['execution']['contract']=='application-execution-v9'
    assert not first['execution']['result_complete'] and final['execution']['result_complete']
    assert json.loads(subprocess.check_output([str(binary),'replay',str(resumed)],text=True))==final['result']
    print('  Teilstapel und Fortsetzung mit unverändertem nativen Nachweis geprüft.')
    print('  Inhaltsaufbau zwischen Grid-Datensätzen und äußere Prüfsumme unterbrechbar.')

print('\nCore 0.61 — Erfahrungsimport im Erstlauf mit eigenen Abbruchpunkten')
with tempfile.TemporaryDirectory(prefix='bruce-import-cancel-demo-') as directory:
    root=Path(directory);train=root/'train';hold=root/'hold';source=root/'source';store=root/'store';target=root/'result'
    train.write_text('x,y\n0,0\n1,1\n2,2\n');hold.write_text('x,y\n3,3\n')
    args=[str(binary),'research','--train',str(train),'--holdout',str(hold),'--domain','rational',
        '--strategy','portfolio-rational','--min','-1','--max','1','--max-denominator','1','--max-power','2','--max-degree','2']
    subprocess.check_output(args+['--planner','portfolio','--output',str(source)],text=True)
    subprocess.check_output([str(binary),'experience','add','--from-run',str(source),'--output',str(store)],text=True)
    train.write_text('x,y\n4,4\n5,5\n6,6\n');hold.write_text('x,y\n7,7\n')
    out=json.loads(subprocess.check_output(args+['--planner','experience-order','--experience',str(store),
        '--verification','replay','--execution-cancellation','experience-checkpoints',
        '--verification-cancellation','research-checkpoints','--serialization-cancellation','digest-checkpoints',
        '--output',str(target)],text=True))
    assert out['execution']['contract']=='application-execution-v10'
    assert out['execution']['execution_cancellation']=='experience-checkpoints'
    assert out['result']['experience_records_used']==1
    assert json.loads(subprocess.check_output([str(binary),'replay',str(target)],text=True))==out['result']
    print('  Erfahrungsimport, äußere Prüfsumme und Replay separat an Abbruchprüfungen angebunden.')
    print('  Unabhängiger Folgeauftrag nutzt die geprüfte Erfahrungsstimme; Nachweis reproduziert.')

print('\nCore 0.62 — Grid-Eingangsquellen im Erstlauf mit Abbruchpunkten')
with tempfile.TemporaryDirectory(prefix='bruce-grid-import-demo-') as directory:
    root=Path(directory);train=root/'train';hold=root/'hold';target=root/'result'
    subprocess.check_output([str(binary),'grid-batch','--paths','0,2;1,2;2,2','--output',str(train)],text=True)
    subprocess.check_output([str(binary),'grid-batch','--paths','3,2','--output',str(hold)],text=True)
    out=json.loads(subprocess.check_output([str(binary),'research','--grid-train',str(train),'--grid-holdout',str(hold),
        '--domain','rational','--strategy','polynomial','--planner','interpolate','--max-degree','2',
        '--verification','replay','--execution-cancellation','grid-source-checkpoints',
        '--verification-cancellation','research-checkpoints','--output',str(target)],text=True))
    assert out['execution']['contract']=='application-execution-v11'
    assert out['execution']['execution_cancellation']=='grid-source-checkpoints'
    assert json.loads(subprocess.check_output([str(binary),'replay',str(target)],text=True))==out['result']
    print('  Gridquellen und Sampleprojektion im Erstlauf separat unterbrechbar.')
    print('  Formelforschung vollständig reproduziert; Quellenprüfung bleibt erhalten.')

print('\nCore 0.63 — Forschungsimport der Z-Auswahl mit Abbruchpunkten')
with tempfile.TemporaryDirectory(prefix='bruce-z-import-demo-') as directory:
    root=Path(directory);train=root/'train';rules=root/'rules';source=root/'source';target=root/'selection'
    train.write_text('x,y\n0,0\n1,1\n');rules.write_text('BRUCE_RULES_V1\nsnapshot demo\n')
    subprocess.check_output([str(binary),'research','--train',str(train),'--min','-1','--max','1','--output',str(source)],text=True)
    out=json.loads(subprocess.check_output([str(binary),'compatibility','--rules',str(rules),'--from-run',str(source),'--at','2',
        '--verification','replay','--execution-cancellation','compatibility-source-checkpoints',
        '--verification-cancellation','compatibility-checkpoints','--output',str(target)],text=True))
    assert out['execution']['contract']=='application-execution-v12'
    assert out['execution']['execution_cancellation']=='compatibility-source-checkpoints'
    assert out['result']['status']=='SELECTED'
    assert json.loads(subprocess.check_output([str(binary),'replay',str(target)],text=True))==out['result']
    print('  Forschungsquelle geprüft, Kandidat projiziert und Auswahl reproduziert.')
    print('  Quellenimport und Kandidatenbewertung besitzen getrennte Abbruchpfade.')

print('\nCore 0.64 — Native Fortsetzungsimporte mit Abbruchpunkten')
with tempfile.TemporaryDirectory(prefix='bruce-resume-import-demo-') as directory:
    root=Path(directory);parent=root/'parent';target=root/'continued'
    subprocess.check_output([str(binary),'grid-batch','--paths','2,6;2,6;4,3','--max-query-steps','1','--output',str(parent)],text=True)
    out=json.loads(subprocess.check_output([str(binary),'grid-resume','--from-run',str(parent),'--additional-queries','2',
        '--verification','replay','--execution-cancellation','resume-import-checkpoints',
        '--verification-cancellation','grid-query-boundaries','--serialization-cancellation','grid-record-checkpoints',
        '--output',str(target)],text=True))
    assert out['execution']['contract']=='application-execution-v13'
    assert out['execution']['execution_cancellation']=='resume-import-checkpoints'
    assert out['result']['batch']['cache']['hits']==1
    assert json.loads(subprocess.check_output([str(binary),'replay',str(target)],text=True))==out['result']
    print('  Vorgänger geprüft, Cache aufgebaut und neue Abfragen reproduziert.')
    print('  Importabbruch und native Abfragegrenze bleiben getrennt.')

print('\nCore 0.65 — Gemeinsames Prüfpunktbudget')
with tempfile.TemporaryDirectory(prefix='bruce-session-budget-demo-') as directory:
    target=Path(directory)/'checked'
    args=[str(binary),'grid-batch','--paths','2,6;4,3','--verification','replay']
    out=json.loads(subprocess.check_output(args+['--max-session-checkpoints','1000','--output',str(target)],text=True))
    usage=out['execution']['session_budget']
    assert out['execution']['contract']=='application-execution-v14'
    assert usage['checkpoints']==sum(usage['by_phase'].values())
    assert json.loads(subprocess.check_output([str(binary),'replay',str(target)],text=True))==out['result']
    insufficient=subprocess.run(args+['--max-session-checkpoints',str(usage['checkpoints']-1)],capture_output=True,text=True)
    assert insufficient.returncode==2
    failure=json.loads(insufficient.stderr)['execution']
    assert failure['failed_at']=='REPLAYING' and failure['contract']=='application-execution-failure-v2'
    assert failure['session_budget']['checkpoints']==usage['checkpoints']-1
    print('  Verbrauch über alle Phasen:',usage['checkpoints'],'Prüfpunkte;',usage['by_phase'])
    print('  Ein Punkt zu wenig: terminaler Replayfehler mit Verbrauch statt Ergebnisfreigabe.')
