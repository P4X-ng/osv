from osv.modules import api
from osv.modules.filemap import FileMap
import subprocess

api.require('java')

javac_with_version = subprocess.check_output(['javac', '-version'], stderr=subprocess.STDOUT).decode('utf-8')

usr_files = FileMap()
_jar = '/tests/java/tests.jar'

if javac_with_version.startswith('javac 1.8'):
    usr_files.add('${OSV_BASE}/modules/java-tests/tests/target/runjava-tests.jar').to(_jar)

    usr_files.add('${OSV_BASE}/modules/java-tests/tests/target/classes/tests/ClassPutInRoot.class').to('/tests/ClassPutInRoot.class')

    usr_files.add('${OSV_BASE}/modules/java-tests/tests-jre-extension/target/tests-jre-extension.jar') \
        .to('/usr/lib/jvm/java/jre/lib/ext/tests-jre-extension.jar')
else:
    usr_files.add('${OSV_BASE}/modules/java-tests/tests-for-java9_1x/target/runjava-9-1x-tests.jar').to(_jar)
