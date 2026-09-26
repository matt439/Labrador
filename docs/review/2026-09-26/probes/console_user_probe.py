from datetime import datetime, timezone
from tools.tests.test_cloud_performance import config
from tools.cloud_performance.common import validate_config
from tools.cloud_performance.infrastructure import template
now = datetime(2030, 1, 1, tzinfo=timezone.utc)
document = config(now)
document['authorization']['allow_launch'] = True
validated = validate_config(document, now=now)
rendered = template(document, require_authorization=True, now=now)
print('Validated AMI tags:', sorted(validated['expected_ami_tags']))
print('Authorized template Runner:', rendered['Resources']['Runner']['Type'])
print('ConsoleUser:', validated['expected_ami_tags'].get('ConsoleUser'))
